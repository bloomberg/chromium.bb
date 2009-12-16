// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_UTIL_H_

#include <set>
#include <string>

#include "base/file_path.h"
#include "chrome/common/extensions/extension.h"

class ExtensionMessageBundle;

// Utilties for manipulating the on-disk storage of extensions.
namespace extension_file_util {

// The name of the directory inside the profile that we store installed
// extension in.
extern const char kInstallDirectoryName[];

// The name of the file that contains the current version of an installed
// extension.
extern const char kCurrentVersionFileName[];

// Move source_dir to dest_dir (it will actually be named dest_dir, not inside
// dest_dir). If the parent path doesn't exixt, create it. If something else is
// already there, remove it.
bool MoveDirSafely(const FilePath& source_dir, const FilePath& dest_dir);

// Updates the Current Version file inside the installed extension.
bool SetCurrentVersion(const FilePath& dest_dir,
                       const std::string& version,
                       std::string* error);

// Reads the Current Version file.
bool ReadCurrentVersion(const FilePath& dir, std::string* version_string);

// Determine what type of install it is (new, upgrade, overinstall, downgrade)
// given the current version and a newly installing version. |extensions_dir| is
// the root directory containing all extensions in the user profile.
// |extension_id| and |current_version_str| are the id and version of the
// extension contained in |src_dir|, if any.
//
// Returns the full path to the destination version directory and the type of
// install that was attempted.
Extension::InstallType CompareToInstalledVersion(
    const FilePath& extensions_dir,
    const std::string& extension_id,
    const std::string& current_version_str,
    const std::string& new_version_str,
    FilePath* version_dir);

// Sanity check that the directory has the minimum files to be a working
// extension.
bool SanityCheckExtension(const FilePath& extension_root);

// Installs an extension unpacked in |src_dir|.
//
// On failure, also returns an error message.
//
// NOTE: |src_dir| is not actually copied in the case of downgrades or
// overinstall of previous verisons of the extension. In that case, the function
// returns true and install_type is populated.
bool InstallExtension(const FilePath& src_dir,
                      const FilePath& version_dir,
                      std::string* error);

// Loads and validates an extension from the specified directory. Returns NULL
// on failure, with a description of the error in |error|.
Extension* LoadExtension(const FilePath& extension_root,
                         bool require_key,
                         std::string* error);

// Returns true if the given extension object is valid and consistent.
// Otherwise, a description of the error is returned in |error|.
bool ValidateExtension(Extension* extension, std::string* error);

// Uninstalls the extension |id| from the install directory |extensions_dir|.
void UninstallExtension(const std::string& id, const FilePath& extensions_dir);

// Clean up directories that aren't valid extensions from the install directory.
void GarbageCollectExtensions(
    const FilePath& extensions_dir,
    const std::set<std::string>& installed_ids,
    const std::map<std::string, std::string>& installed_versions);

// Loads extension message catalogs and returns message bundle.
// Returns NULL on error, or if extension is not localized.
ExtensionMessageBundle* LoadExtensionMessageBundle(
    const FilePath& extension_path,
    const DictionaryValue& manifest,
    std::string* error);

// We need to reserve the namespace of entries that start with "_" for future
// use by Chrome.
// If any files or directories are found using "_" prefix and are not on
// reserved list we return false, and set error message.
bool CheckForIllegalFilenames(const FilePath& extension_path,
                              std::string* error);

}  // extension_file_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_UTIL_H_
