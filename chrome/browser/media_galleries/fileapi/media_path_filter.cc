// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"

#include <algorithm>
#include <string>

#include "base/strings/string_util.h"
#include "net/base/mime_util.h"

namespace {

const base::FilePath::CharType* const kExtraSupportedExtensions[] = {
  // RAW picture file types.
  // Some of which are just image/tiff.
  FILE_PATH_LITERAL("3fr"),  // (Hasselblad)
  FILE_PATH_LITERAL("arw"),  // (Sony)
  FILE_PATH_LITERAL("dcr"),  // (Kodak)
  FILE_PATH_LITERAL("dng"),  // (Adobe, Leica, Ricoh, Samsung)
  FILE_PATH_LITERAL("erf"),  // (Epson)
  FILE_PATH_LITERAL("k25"),  // (Kodak)
  FILE_PATH_LITERAL("kdc"),  // (Kodak)
  FILE_PATH_LITERAL("mef"),  // (Mamiya)
  FILE_PATH_LITERAL("mos"),  // (Leaf)
  FILE_PATH_LITERAL("nef"),  // (Nikon)
  FILE_PATH_LITERAL("pef"),  // (Pentax)
  FILE_PATH_LITERAL("sr2"),  // (Sony)
  FILE_PATH_LITERAL("srf"),  // (Sony)

  // More RAW picture file types.
  FILE_PATH_LITERAL("cr2"),  // (Canon - image/x-canon-cr2)
  // Note, some .crw files are just TIFFs.
  FILE_PATH_LITERAL("crw"),  // (Canon - image/x-canon-crw)
  FILE_PATH_LITERAL("mrw"),  // (Minolta - image/x-minolta-mrw)
  FILE_PATH_LITERAL("orf"),  // (Olympus - image/x-olympus-orf)
  FILE_PATH_LITERAL("raf"),  // (Fuji)
  FILE_PATH_LITERAL("rw2"),  // (Panasonic - image/x-panasonic-raw)
  FILE_PATH_LITERAL("x3f"),  // (Sigma - image/x-x3f)

  // There exists many file formats all with the .raw extension. For now, only
  // the following types are supported:
  // - TIFF files with .raw extension - image/tiff
  // - Leica / Panasonic RAW files - image/x-panasonic-raw
  // - Phase One RAW files - image/x-phaseone-raw
  FILE_PATH_LITERAL("raw"),

  // Video files types.
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

  // Audio file types. Many of these file types are audio files in the same
  // containers that the MIME sniffer already detects as video/subtype.
  FILE_PATH_LITERAL("aac"),   // audio/mpeg
  FILE_PATH_LITERAL("alac"),  // video/mp4
  FILE_PATH_LITERAL("flac"),  // audio/x-flac
  FILE_PATH_LITERAL("m4b"),   // video/mp4
  FILE_PATH_LITERAL("m4p"),   // video/mp4
  FILE_PATH_LITERAL("wma"),   // video/x-ms-asf
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
