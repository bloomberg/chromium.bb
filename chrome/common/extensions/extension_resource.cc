// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_resource.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

ExtensionResource::ExtensionResource() {
}

ExtensionResource::ExtensionResource(const std::string& extension_id,
                                     const FilePath& extension_root,
                                     const FilePath& relative_path)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      relative_path_(relative_path) {
}

ExtensionResource::~ExtensionResource() {}

const FilePath& ExtensionResource::GetFilePath() const {
  if (extension_root_.empty() || relative_path_.empty()) {
    DCHECK(full_resource_path_.empty());
    return full_resource_path_;
  }

  // We've already checked, just return last value.
  if (!full_resource_path_.empty())
    return full_resource_path_;

  full_resource_path_ =
      GetFilePath(extension_root_, relative_path_);
  return full_resource_path_;
}

// static
FilePath ExtensionResource::GetFilePath(
    const FilePath& extension_root, const FilePath& relative_path) {
  // We need to resolve the parent references in the extension_root
  // path on its own because IsParent doesn't like parent references.
  FilePath clean_extension_root(extension_root);
  if (!file_util::AbsolutePath(&clean_extension_root))
    return FilePath();

  FilePath full_path = clean_extension_root.Append(relative_path);

  // We must resolve the absolute path of the combined path when
  // the relative path contains references to a parent folder (i.e., '..').
  // We also check if the path exists because the posix version of AbsolutePath
  // will fail if the path doesn't exist, and we want the same behavior on
  // Windows... So until the posix and Windows version of AbsolutePath are
  // unified, we need an extra call to PathExists, unfortunately.
  // TODO(mad): Fix this once AbsolutePath is unified.
  if (file_util::AbsolutePath(&full_path) &&
      file_util::PathExists(full_path) &&
      clean_extension_root.IsParent(full_path)) {
    return full_path;
  }

  return FilePath();
}

// Unit-testing helpers.
FilePath::StringType ExtensionResource::NormalizeSeperators(
    const FilePath::StringType& path) const {
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  FilePath::StringType win_path = path;
  for (size_t i = 0; i < win_path.length(); i++) {
    if (FilePath::IsSeparator(win_path[i]))
      win_path[i] = FilePath::kSeparators[0];
  }
  return win_path;
#else
  return path;
#endif  // FILE_PATH_USES_WIN_SEPARATORS
}

bool ExtensionResource::ComparePathWithDefault(const FilePath& path) const {
  // Make sure we have a cached value to test against...
  if (full_resource_path_.empty())
    GetFilePath();
  if (NormalizeSeperators(path.value()) ==
    NormalizeSeperators(full_resource_path_.value())) {
    return true;
  } else {
    return false;
  }
}
