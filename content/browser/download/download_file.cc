// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file.h"

#include "base/file_util.h"
#include "base/stringprintf.h"

namespace {

  // The maximum number of 'uniquified' files we will try to create.
// This is used when the filename we're trying to download is already in use,
// so we create a new unique filename by appending " (nnn)" before the
// extension, where 1 <= nnn <= kMaxUniqueFiles.
// Also used by code that cleans up said files.
static const int kMaxUniqueFiles = 100;

}

// static
void DownloadFile::AppendNumberToPath(FilePath* path, int number) {
  *path = path->InsertBeforeExtensionASCII(StringPrintf(" (%d)", number));
}

// static
FilePath DownloadFile::AppendSuffixToPath(
    const FilePath& path,
    const FilePath::StringType& suffix) {
  FilePath::StringType file_name;
  base::SStringPrintf(
      &file_name, PRFilePathLiteral PRFilePathLiteral, path.value().c_str(),
      suffix.c_str());
  return FilePath(file_name);
}

// static
int DownloadFile::GetUniquePathNumber(const FilePath& path) {
  if (!file_util::PathExists(path))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path))
      return count;
  }

  return -1;
}

// static
int DownloadFile::GetUniquePathNumberWithSuffix(
    const FilePath& path,
    const FilePath::StringType& suffix) {
  if (!file_util::PathExists(path) &&
      !file_util::PathExists(AppendSuffixToPath(path, suffix)))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path) &&
        !file_util::PathExists(AppendSuffixToPath(new_path, suffix)))
      return count;
  }

  return -1;
}
