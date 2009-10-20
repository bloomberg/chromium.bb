// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_RESOURCE_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_RESOURCE_H_

#include "base/file_path.h"

// Represents a resource inside an extension. For example, an image, or a
// JavaScript file. This is more complicated than just a simple FilePath
// because extension resources can come from multiple physical file locations
// depending on locale.
class ExtensionResource {
 public:
  ExtensionResource();

  ExtensionResource(const FilePath& extension_root,
                    const FilePath& relative_path);

  // Returns actual path to the resource (default or locale specific).
  // *** MIGHT HIT FILESYSTEM. Do not call on UI thread! ***
  const FilePath& GetFilePath() const;

  // Static version to avoid creating an instance of ExtensionResource
  // when all we want is the localization code.
  // *** MIGHT HIT FILESYSTEM. Do not call on UI thread! ***
  static FilePath GetFilePath(const FilePath& extension_root,
                              const FilePath& relative_path);

  // Getters
  const FilePath& extension_root() const { return extension_root_; }
  const FilePath& relative_path() const { return relative_path_; }

  // Unittest helpers.
  FilePath::StringType NormalizeSeperators(FilePath::StringType path) const;
  bool ComparePathWithDefault(const FilePath& path) const;

 private:
  // Extension root.
  FilePath extension_root_;

  // Relative path to resource.
  FilePath relative_path_;

  // Full path to extension resource. Starts empty.
  mutable FilePath full_resource_path_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_RESOURCE_H_
