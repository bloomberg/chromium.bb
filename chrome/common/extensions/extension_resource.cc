// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_resource.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_l10n_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

ExtensionResource::ExtensionResource() {
}

ExtensionResource::ExtensionResource(const FilePath& extension_root,
                                     const FilePath& relative_path)
    : extension_root_(extension_root),
      relative_path_(relative_path) {
}

const FilePath& ExtensionResource::GetFilePath() const {
  if (extension_root_.empty() || relative_path_.empty()) {
    DCHECK(full_resource_path_.empty());
    return full_resource_path_;
  }

  // We've already checked, just return last value.
  if (!full_resource_path_.empty())
    return full_resource_path_;

  full_resource_path_ = GetFilePath(extension_root_, relative_path_);
  return full_resource_path_;
}

// Static version...
FilePath ExtensionResource::GetFilePath(const FilePath& extension_root,
                                        const FilePath& relative_path) {
  std::vector<FilePath> l10n_relative_paths;
  extension_l10n_util::GetL10nRelativePaths(relative_path,
                                            &l10n_relative_paths);
  // We need to resolve the parent references in the extension_root
  // path on its own because IsParent doesn't like parent references.
  FilePath clean_extension_root(extension_root);
  if (!file_util::AbsolutePath(&clean_extension_root))
    return FilePath();

  // Stat l10n file(s), and return new path if it exists.
  for (size_t i = 0; i < l10n_relative_paths.size(); ++i) {
    FilePath full_path = clean_extension_root.Append(l10n_relative_paths[i]);
    if (file_util::AbsolutePath(&full_path) &&
        clean_extension_root.IsParent(full_path) &&
        file_util::PathExists(full_path)) {
      return full_path;
    }
  }

  // Fall back to root resource.
  FilePath full_path = clean_extension_root.Append(relative_path);
  if (file_util::AbsolutePath(&full_path) &&
      clean_extension_root.IsParent(full_path)) {
    return full_path;
  }

  return FilePath();
}

// Unittesting helpers.
FilePath::StringType ExtensionResource::NormalizeSeperators(
    FilePath::StringType path) const {
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  FilePath::StringType ret_val;
  for (size_t i = 0; i < path.length(); i++) {
    if (FilePath::IsSeparator(path[i]))
      path[i] = FilePath::kSeparators[0];
  }
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  return path;
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
