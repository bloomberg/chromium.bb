// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"

// This class holds the list of file path extensions that we should expose on
// media filesystem.
class MediaPathFilter {
 public:
  MediaPathFilter();
  ~MediaPathFilter();
  bool Match(const base::FilePath& path);

 private:
  typedef std::vector<base::FilePath::StringType> MediaFileExtensionList;

  void EnsureInitialized();

  // Checks |initialized_| is only accessed on one sequence.
  base::SequenceChecker sequence_checker_;
  bool initialized_;
  MediaFileExtensionList media_file_extensions_;

  DISALLOW_COPY_AND_ASSIGN(MediaPathFilter);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_PATH_FILTER_H_
