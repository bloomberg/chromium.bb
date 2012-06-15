// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
#pragma once

#include <set>

#include "base/file_path.h"

class ExtensionService;

// Garbage collection for extensions. This will delete any directories in the
// installation directory of |extension_service_| that aren't valid, as well
// as removing any references in the preferences to extensions that are
// missing local content.
namespace extensions {

class ExtensionGarbageCollector {
 public:
  explicit ExtensionGarbageCollector(ExtensionService* extension_service);
  virtual ~ExtensionGarbageCollector();

  // Begin garbage collection; fetch the installed extensions' ids.
  void GarbageCollectExtensions();

 private:
  // Check the installation directory for directories with invalid extension
  // ids, deleting them if found. Call CheckExtensionPreferences with a list of
  // present directories.
  void CheckExtensionDirectoriesOnBackgroundThread(
      const FilePath& installation_directory);

  // Check for any extension directories which:
  //   - Are an old version of a current extension;
  //   - Are present in the preferences but not on the file system;
  //   - Are present on the file system, but not in the preferences
  // and delete them if found.
  void CheckExtensionPreferences(const std::set<FilePath>& extension_paths);

  // Helper function to cleanup any old versions of an extension in the
  // extension's base directory.
  void CleanupOldVersionsOnBackgroundThread(
      const FilePath& path, const FilePath& current_version);

  ExtensionService* extension_service_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_H_
