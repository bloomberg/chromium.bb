// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UNPACKED_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_UNPACKED_INSTALLER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

class ExtensionService;

namespace extensions {

class Extension;
class RequirementsChecker;

// Installs and loads an unpacked extension. Because internal state needs to be
// held about the instalation process, only one call to Load*() should be made
// per UnpackedInstaller.
// TODO(erikkay): It might be useful to be able to load a packed extension
// (presumably into memory) without installing it.
class UnpackedInstaller
    : public base::RefCountedThreadSafe<UnpackedInstaller> {
 public:
  static scoped_refptr<UnpackedInstaller> Create(
      ExtensionService* extension_service);

  // Loads the extension from the directory |extension_path|, which is
  // the top directory of a specific extension where its manifest file lives.
  // Errors are reported through ExtensionErrorReporter. On success,
  // ExtensionService::AddExtension() is called.
  void Load(const FilePath& extension_path);

  // Loads the extension from the directory |extension_path|;
  // for use with command line switch --load-extension=path.
  // This is equivalent to Load, except that it runs synchronously.
  void LoadFromCommandLine(const FilePath& extension_path);

  // Allows prompting for plugins to be disabled; intended for testing only.
  bool prompt_for_plugins() { return prompt_for_plugins_; }
  void set_prompt_for_plugins(bool val) { prompt_for_plugins_ = val; }

  // Allows overriding of whether modern manifest versions are required;
  // intended for testing.
  bool require_modern_manifest_version() const {
    return require_modern_manifest_version_;
  }
  void set_require_modern_manifest_version(bool val) {
    require_modern_manifest_version_ = val;
  }

 private:
  friend class base::RefCountedThreadSafe<UnpackedInstaller>;

  explicit UnpackedInstaller(ExtensionService* extension_service);
  virtual ~UnpackedInstaller();

  // Must be called from the UI thread.
  void CheckRequirements();

  // Callback from RequirementsChecker.
  void OnRequirementsChecked(std::vector<std::string> requirement_errors);

  // Verifies if loading unpacked extensions is allowed.
  bool IsLoadingUnpackedAllowed() const;

  // We change the input extension path to an absolute path, on the file thread.
  // Then we need to check the file access preference, which needs
  // to happen back on the UI thread, so it posts CheckExtensionFileAccess on
  // the UI thread. In turn, once that gets the pref, it goes back to the
  // file thread with LoadWithFileAccess.
  // TODO(yoz): It would be nice to remove this ping-pong, but we need to know
  // what file access flags to pass to extension_file_util::LoadExtension.
  void GetAbsolutePath();
  void CheckExtensionFileAccess();
  void LoadWithFileAccess(int flags);

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const std::string& error);

  // Called when an unpacked extension has been loaded and installed.
  void OnLoaded();

  // Helper to get the Extension::CreateFlags for the installing extension.
  int GetFlags();

  base::WeakPtr<ExtensionService> service_weak_;

  // The pathname of the directory to load from, which is an absolute path
  // after GetAbsolutePath has been called.
  FilePath extension_path_;

  // If true and the extension contains plugins, we prompt the user before
  // loading.
  bool prompt_for_plugins_;

  scoped_ptr<RequirementsChecker> requirements_checker_;

  scoped_refptr<const Extension> extension_;

  // Whether to require the extension installed to have a modern manifest
  // version.
  bool require_modern_manifest_version_;

  DISALLOW_COPY_AND_ASSIGN(UnpackedInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UNPACKED_INSTALLER_H_
