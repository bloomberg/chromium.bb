// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"

#include <algorithm>
#include <string>

#include "base/string_util.h"
#include "net/base/mime_util.h"

namespace chrome {

namespace {

const base::FilePath::CharType* const kExtraSupportedExtensions[] = {
  FILE_PATH_LITERAL("3gp"),
  FILE_PATH_LITERAL("3gpp"),
  FILE_PATH_LITERAL("avi"),
  FILE_PATH_LITERAL("flv"),
  FILE_PATH_LITERAL("mov"),
  FILE_PATH_LITERAL("mpeg"),
  FILE_PATH_LITERAL("mpeg4"),
  FILE_PATH_LITERAL("mpegps"),
  FILE_PATH_LITERAL("mpg"),
  FILE_PATH_LITERAL("wmv"),
};

bool IsUnsupportedExtension(const base::FilePath::StringType& extension) {
  std::string mime_type;
  return !net::GetMimeTypeFromExtension(extension, &mime_type) ||
      !net::IsSupportedMimeType(mime_type);
}

}  // namespace

MediaPathFilter::MediaPathFilter()
    : initialized_(false) {
}

MediaPathFilter::~MediaPathFilter() {
}

bool MediaPathFilter::Match(const base::FilePath& path) {
  EnsureInitialized();
  return std::binary_search(media_file_extensions_.begin(),
                            media_file_extensions_.end(),
                            StringToLowerASCII(path.Extension()));
}

void MediaPathFilter::EnsureInitialized() {
  if (initialized_)
    return;

  base::AutoLock lock(initialization_lock_);
  if (initialized_)
    return;

  net::GetExtensionsForMimeType("image/*", &media_file_extensions_);
  net::GetExtensionsForMimeType("audio/*", &media_file_extensions_);
  net::GetExtensionsForMimeType("video/*", &media_file_extensions_);

  MediaFileExtensionList::iterator new_end =
      std::remove_if(media_file_extensions_.begin(),
                     media_file_extensions_.end(),
                     &IsUnsupportedExtension);
  media_file_extensions_.erase(new_end, media_file_extensions_.end());

  // Add other common extensions.
  for (size_t i = 0; i < arraysize(kExtraSupportedExtensions); ++i)
    media_file_extensions_.push_back(kExtraSupportedExtensions[i]);

  for (MediaFileExtensionList::iterator itr = media_file_extensions_.begin();
       itr != media_file_extensions_.end(); ++itr)
    *itr = base::FilePath::kExtensionSeparator + *itr;
  std::sort(media_file_extensions_.begin(), media_file_extensions_.end());

  initialized_ = true;
}

}  // namespace chrome
