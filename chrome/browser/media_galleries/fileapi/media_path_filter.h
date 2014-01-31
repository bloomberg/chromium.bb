// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media_galleries/media_scan_types.h"

// This class holds the list of file path extensions that we should expose on
// media filesystem.
class MediaPathFilter {
 public:
  // Used to skip hidden folders and files. Returns true if the file specified
  // by |path| should be skipped.
  static bool ShouldSkip(const base::FilePath& path);

  MediaPathFilter();
  ~MediaPathFilter();

  // Returns true if |path| is a media file.
  bool Match(const base::FilePath& path);

  // Returns the type of |path| or MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN if it
  // is not a media file.
  MediaGalleryScanFileType GetType(const base::FilePath& path);

 private:
  typedef std::vector<base::FilePath::StringType> MediaFileExtensionList;

  // Key: .extension
  // Value: MediaGalleryScanFileType, but stored as an int to allow "|="
  typedef base::hash_map<base::FilePath::StringType, int> MediaFileExtensionMap;

  void EnsureInitialized();

  void AddExtensionsToMediaFileExtensionMap(
      const MediaFileExtensionList& extensions_list,
      MediaGalleryScanFileType type);
  void AddAdditionalExtensionsToMediaFileExtensionMap(
      const base::FilePath::CharType* const* extensions_list,
      size_t extensions_list_size,
      MediaGalleryScanFileType type);
  void AddExtensionToMediaFileExtensionMap(
      const base::FilePath::CharType* extension,
      MediaGalleryScanFileType type);

  // Checks |initialized_| is only accessed on one sequence.
  base::SequenceChecker sequence_checker_;
  bool initialized_;
  MediaFileExtensionMap media_file_extensions_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaPathFilter);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_
