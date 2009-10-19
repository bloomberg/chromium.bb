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
  if (extension_root_.empty() || relative_path_.empty())
    return full_resource_path_;

  // We've already checked, just return last value.
  if (!full_resource_path_.empty())
    return full_resource_path_;

  // Stat l10n file, and return new path if it exists.
  FilePath l10n_relative_path =
    extension_l10n_util::GetL10nRelativePath(relative_path_);
  full_resource_path_ = CombinePathsSafely(extension_root_, l10n_relative_path);
  if (file_util::PathExists(full_resource_path_)) {
    return full_resource_path_;
  }

  // Fall back to root resource.
  full_resource_path_ = CombinePathsSafely(extension_root_, relative_path_);
  return full_resource_path_;
}

FilePath ExtensionResource::CombinePathsSafely(
    const FilePath& extension_path,
    const FilePath& relative_resource_path) const {
  // Build up a file:// URL and convert that back to a FilePath.  This avoids
  // URL encoding and path separator issues.

  // Convert the extension's root to a file:// URL.
  GURL extension_url = net::FilePathToFileURL(extension_path);
  if (!extension_url.is_valid())
    return FilePath();

  // Append the requested path.
  std::string relative_path =
    WideToUTF8(relative_resource_path.ToWStringHack());
  GURL::Replacements replacements;
  std::string new_path(extension_url.path());
  new_path += "/";
  new_path += relative_path;
  replacements.SetPathStr(new_path);
  GURL file_url = extension_url.ReplaceComponents(replacements);
  if (!file_url.is_valid())
    return FilePath();

  // Convert the result back to a FilePath.
  FilePath ret_val;
  if (!net::FileURLToFilePath(file_url, &ret_val))
    return FilePath();

  // Converting the extension_url back to a path removes all .. and . references
  // that may have been in extension_path that would cause isParent to break.
  FilePath sanitized_extension_path;
  if (!net::FileURLToFilePath(extension_url, &sanitized_extension_path))
    return FilePath();

  // Double-check that the path we ended up with is actually inside the
  // extension root.
  if (!sanitized_extension_path.IsParent(ret_val))
    return FilePath();

  return ret_val;
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
  if (NormalizeSeperators(path.value()) ==
    NormalizeSeperators(full_resource_path_.value())) {
    return true;
  } else {
    return false;
  }
}
