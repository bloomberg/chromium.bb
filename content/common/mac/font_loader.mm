// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/font_loader.h"

#import <Cocoa/Cocoa.h>
#include <CoreText/CoreText.h>

#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/mac/font_descriptor.h"

namespace content {
namespace {

std::unique_ptr<FontLoader::ResultInternal> LoadFontOnFileThread(
    const FontDescriptor& font) {
  base::AssertBlockingAllowed();

  NSFont* font_to_encode = font.ToNSFont();

  // Load appropriate NSFont.
  if (!font_to_encode) {
    DLOG(ERROR) << "Failed to load font " << font.font_name;
    return nullptr;
  }

  // NSFont -> File path.
  // Warning: Calling this function on a font activated from memory will result
  // in failure with a -50 - paramErr.  This may occur if
  // CreateCGFontFromBuffer() is called in the same process as this function
  // e.g. when writing a unit test that exercises these two functions together.
  // If said unit test were to load a system font and activate it from memory
  // it becomes impossible for the system to the find the original file ref
  // since the font now lives in memory as far as it's concerned.
  CTFontRef ct_font = base::mac::NSToCFCast(font_to_encode);
  base::scoped_nsobject<NSURL> font_url(
      base::mac::CFToNSCast(base::mac::CFCastStrict<CFURLRef>(
          CTFontCopyAttribute(ct_font, kCTFontURLAttribute))));
  if (![font_url isFileURL]) {
    DLOG(ERROR) << "Failed to find font file for " << font.font_name;
    return nullptr;
  }

  base::FilePath font_path = base::mac::NSStringToFilePath([font_url path]);

  // Load file into shared memory buffer.
  int64_t font_file_size_64 = -1;
  if (!base::GetFileSize(font_path, &font_file_size_64)) {
    DLOG(ERROR) << "Couldn't get font file size for " << font_path.value();
    return nullptr;
  }

  if (font_file_size_64 <= 0 ||
      font_file_size_64 >= std::numeric_limits<int32_t>::max()) {
    DLOG(ERROR) << "Bad size for font file " << font_path.value();
    return nullptr;
  }

  auto result = base::MakeUnique<FontLoader::ResultInternal>();

  int32_t font_file_size_32 = static_cast<int32_t>(font_file_size_64);
  if (!result->font_data.CreateAndMapAnonymous(font_file_size_32)) {
    DLOG(ERROR) << "Failed to create shmem area for " << font.font_name;
    return nullptr;
  }

  int32_t amt_read = base::ReadFile(
      font_path, reinterpret_cast<char*>(result->font_data.memory()),
      font_file_size_32);
  if (amt_read != font_file_size_32) {
    DLOG(ERROR) << "Failed to read font data for " << font_path.value();
    return nullptr;
  }

  result->font_data_size = font_file_size_32;

  // Font loading used to call ATSFontGetContainer() and used that as font id.
  // ATS is deprecated. CoreText offers up the ATSFontRef typeface ID via
  // CTFontGetPlatformFont.
  result->font_id = CTFontGetPlatformFont(ct_font, nil);
  DCHECK_NE(0u, result->font_id);

  if (result->font_data_size == 0 || result->font_id == 0)
    return nullptr;

  return result;
}

void ReplyOnUIThread(FontLoader::LoadedCallback callback,
                     std::unique_ptr<FontLoader::ResultInternal> result) {
  if (!result) {
    std::move(callback).Run(0, base::SharedMemoryHandle(), 0);
    return;
  }

  DCHECK_NE(0u, result->font_data_size);
  DCHECK_NE(0u, result->font_id);

  base::SharedMemoryHandle handle = result->font_data.handle().Duplicate();
  result->font_data.Unmap();
  result->font_data.Close();
  std::move(callback).Run(result->font_data_size, handle, result->font_id);
}

}  // namespace

// static
void FontLoader::LoadFont(const FontDescriptor& font, LoadedCallback callback) {
  // Tasks are triggered when font loading in the sandbox fails. Usually due to
  // a user installing a third-party font manager. See crbug.com/72727. Web page
  // rendering can't continue until a font is returned.
  constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, kTraits, base::BindOnce(&LoadFontOnFileThread, font),
      base::BindOnce(&ReplyOnUIThread, std::move(callback)));
}

// static
bool FontLoader::CGFontRefFromBuffer(base::SharedMemoryHandle font_data,
                                     uint32_t font_data_size,
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
  base::ScopedCFTypeRef<CGDataProviderRef> provider(
      CGDataProviderCreateWithCFData(base::mac::NSToCFCast(data)));
  if (!provider)
    return false;

  *out = CGFontCreateWithDataProvider(provider.get());

  if (*out == NULL)
    return false;

  return true;
}

// static
std::unique_ptr<FontLoader::ResultInternal> FontLoader::LoadFontForTesting(
    const FontDescriptor& font) {
  return LoadFontOnFileThread(font);
}

}  // namespace content
