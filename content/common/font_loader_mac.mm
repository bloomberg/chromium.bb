// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_loader_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"

// static
bool FontLoader::LoadFontIntoBuffer(NSFont* font_to_encode,
                                    base::SharedMemory* font_data,
                                    uint32* font_data_size) {
  CHECK(font_data && font_data_size);
  *font_data_size = 0;

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
  return true;
}

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
