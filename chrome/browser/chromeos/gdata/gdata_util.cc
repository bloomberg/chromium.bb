// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include <string>
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"

namespace gdata {
namespace util {

namespace {

const char kGDataMountPointPath[] = "/special/gdata";

const FilePath::CharType* kGDataMountPointPathComponents[] = {
  "/", "special", "gdata"
};

}  // namespace

FilePath GetGDataMountPointPath() {
  return FilePath::FromUTF8Unsafe(kGDataMountPointPath);
}

std::string GetGDataMountPointPathAsString() {
  return kGDataMountPointPath;
}

bool IsUnderGDataMountPoint(const FilePath& path) {
  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  if (components.size() < arraysize(kGDataMountPointPathComponents))
    return false;

  for (size_t i = 0; i < arraysize(kGDataMountPointPathComponents); ++i) {
    if (components[i] != kGDataMountPointPathComponents[i])
      return false;
  }

  return true;
}

FilePath ExtractGDataPath(const FilePath& path) {
  if (!IsUnderGDataMountPoint(path))
    return FilePath();

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'gdata'.
  FilePath extracted;
  for (size_t i = arraysize(kGDataMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
}

}  // namespace util
}  // namespace gdata
