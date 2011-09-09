// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/font_loader.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"

extern "C" {

// Work around http://crbug.com/93191, a really nasty memory smasher bug.
// On Mac OS X 10.7 ("Lion"), ATS writes to memory it doesn't own.
// SendDeactivateFontsInContainerMessage, called by ATSFontDeactivate,
// may trash memory whenever dlsym(RTLD_DEFAULT,
// "_CTFontManagerUnregisterFontForData") returns NULL. In that case, it tries
// to locate that symbol in the CoreText framework, doing some extremely
// sloppy string handling resulting in a likelihood that the string
// "Text.framework/Versions/A/CoreText" will be written over memory that it
// doesn't own. The kicker here is that Apple dlsym always inserts its own
// leading underscore, so ATS actually winds up looking up a
// __CTFontManagerUnregisterFontForData symbol, which doesn't even exist in
// CoreText. It's only got the single-underscore variant corresponding to an
// underscoreless extern "C" name.
//
// Providing a single-underscored extern "C" function by this name results in
// a __CTFontManagerUnregisterFontForData symbol that, as long as it's public
// (not private extern) and unstripped, ATS will find. If it finds it, it
// avoids making amateur string mistakes that ruin everyone else's good time.
//
// Since ATS wouldn't normally be able to call this function anyway, it's just
// left as a no-op here.
//
// This file seems as good as any other to place this function. It was chosen
// because it already interfaces with ATS for other reasons.
//
// SendDeactivateFontsInContainerMessage on 10.6 ("Snow Leopard") appears to
// share this bug but this sort of memory corruption wasn't detected until
// 10.7. The implementation in 10.5 ("Leopard") does not have this problem.
__attribute__((visibility("default")))
void _CTFontManagerUnregisterFontForData(NSUInteger, int) {
}

}  // extern "C"

// static
bool FontLoader::LoadFontIntoBuffer(NSFont* font_to_encode,
                                    base::SharedMemory* font_data,
                                    uint32* font_data_size,
                                    uint32* font_id) {
  CHECK(font_data);
  CHECK(font_data_size);
  CHECK(font_id);
  *font_data_size = 0;
  *font_id = 0;

  // Used only for logging.
  std::string font_name([[font_to_encode fontName] UTF8String]);

  // Load appropriate NSFont.
  if (!font_to_encode) {
    LOG(ERROR) << "Failed to load font " << font_name;
    return false;
  }

  // NSFont -> ATSFontRef.
  ATSFontRef ats_font =
      CTFontGetPlatformFont(reinterpret_cast<CTFontRef>(font_to_encode), NULL);
  if (!ats_font) {
    LOG(ERROR) << "Conversion to ATSFontRef failed for " << font_name;
    return false;
  }

  // Retrieve the ATSFontContainerRef corresponding to the font file we want to
  // load. This is a unique identifier that allows the caller determine if the
  // font file in question is already loaded.
  COMPILE_ASSERT(sizeof(ATSFontContainerRef) == sizeof(font_id),
      uint32_cant_hold_fontcontainer_ref);
  ATSFontContainerRef fontContainer = kATSFontContainerRefUnspecified;
  if (ATSFontGetContainer(ats_font, 0, &fontContainer) != noErr) {
      LOG(ERROR) << "Failed to get font container ref for " << font_name;
      return false;
  }

  // ATSFontRef -> File path.
  // Warning: Calling this function on a font activated from memory will result
  // in failure with a -50 - paramErr.  This may occur if
  // CreateCGFontFromBuffer() is called in the same process as this function
  // e.g. when writing a unit test that exercises these two functions together.
  // If said unit test were to load a system font and activate it from memory
  // it becomes impossible for the system to the find the original file ref
  // since the font now lives in memory as far as it's concerned.
  FSRef font_fsref;
  if (ATSFontGetFileReference(ats_font, &font_fsref) != noErr) {
    LOG(ERROR) << "Failed to find font file for " << font_name;
    return false;
  }
  FilePath font_path = FilePath(base::mac::PathFromFSRef(font_fsref));

  // Load file into shared memory buffer.
  int64 font_file_size_64 = -1;
  if (!file_util::GetFileSize(font_path, &font_file_size_64)) {
    LOG(ERROR) << "Couldn't get font file size for " << font_path.value();
    return false;
  }

  if (font_file_size_64 <= 0 || font_file_size_64 >= kint32max) {
    LOG(ERROR) << "Bad size for font file " << font_path.value();
    return false;
  }

  int32 font_file_size_32 = static_cast<int32>(font_file_size_64);
  if (!font_data->CreateAndMapAnonymous(font_file_size_32)) {
    LOG(ERROR) << "Failed to create shmem area for " << font_name;
    return false;
  }

  int32 amt_read = file_util::ReadFile(font_path,
                       reinterpret_cast<char*>(font_data->memory()),
                       font_file_size_32);
  if (amt_read != font_file_size_32) {
    LOG(ERROR) << "Failed to read font data for " << font_path.value();
    return false;
  }

  *font_data_size = font_file_size_32;
  *font_id = fontContainer;
  return true;
}

// static
bool FontLoader::CGFontRefFromBuffer(base::SharedMemoryHandle font_data,
                                     uint32 font_data_size,
                                     CGFontRef* out) {
  *out = NULL;

  using base::SharedMemory;
  DCHECK(SharedMemory::IsHandleValid(font_data));
  DCHECK_GT(font_data_size, 0U);

  SharedMemory shm(font_data, /*read_only=*/true);
  if (!shm.Map(font_data_size))
    return false;

  NSData* data = [NSData dataWithBytes:shm.memory()
                                length:font_data_size];
  base::mac::ScopedCFTypeRef<CGDataProviderRef> provider(
      CGDataProviderCreateWithCFData(base::mac::NSToCFCast(data)));
  if (!provider)
    return false;

  *out = CGFontCreateWithDataProvider(provider.get());

  if (*out == NULL)
    return false;

  return true;
}

// TODO(jeremy): Remove once http://webk.it/66935 lands.
// static
bool FontLoader::ATSFontContainerFromBuffer(base::SharedMemoryHandle font_data,
                                            uint32 font_data_size,
                                            ATSFontContainerRef* font_container)
{
  CHECK(font_container);

  using base::SharedMemory;
  DCHECK(SharedMemory::IsHandleValid(font_data));
  DCHECK_GT(font_data_size, 0U);

  SharedMemory shm(font_data, true);
  if (!shm.Map(font_data_size))
    return false;

  // A value of 3 means the font is private and can't be seen by anyone else.
  // This is the value used by WebKit when activating remote fonts.
  const ATSFontContext kFontContextPrivate = 3;
  OSStatus err = ATSFontActivateFromMemory(shm.memory(), font_data_size,
                    kFontContextPrivate, kATSFontFormatUnspecified, NULL,
                    kATSOptionFlagsDefault, font_container);
  if (err != noErr || !font_container)
    return false;

  return true;
}
