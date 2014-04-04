// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SHARED_MODULE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_SHARED_MODULE_SERVICE_H_

#include <list>

#include "chrome/browser/extensions/install_observer.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionSet;

class SharedModuleService : public InstallObserver {
 public:
  enum ImportStatus {
    // No imports needed.
    IMPORT_STATUS_OK,

    // Imports are needed, but can be satisfied (i.e., there are missing or
    // outdated imports for a webstore extension).
    IMPORT_STATUS_UNSATISFIED,

    // Imports are needed, and can't be satisfied (i.e., missing or outdated
    // imports for an extension not in the webstore).
    IMPORT_STATUS_UNRECOVERABLE
  };

  explicit SharedModuleService(content::BrowserContext* context);
  virtual ~SharedModuleService();

  // Checks an extension's imports. Imports that are not installed are stored
  // in |missing_modules|, and imports that are out of date are stored in
  // |outdated_modules|.
  ImportStatus CheckImports(
      const extensions::Extension* extension,
      std::list<SharedModuleInfo::ImportInfo>* missing_modules,
      std::list<SharedModuleInfo::ImportInfo>* outdated_modules);

  // Checks an extension's shared module imports to see if they are satisfied.
  // If they are not, this function adds the dependencies to the pending install
  // list if |extension| came from the webstore.
  ImportStatus SatisfyImports(const extensions::Extension* extension);

 private:
  // Returns a set of extensions that import a given extension.
  scoped_ptr<const ExtensionSet> GetDependentExtensions(
      const Extension* extension);

  // Uninstalls shared modules that were only referenced by |extension|.
  void PruneSharedModulesOnUninstall(const Extension* extension);

  // InstallObserver implementation:
  virtual void OnExtensionUninstalled(const Extension* extension) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // The context associated with this SharedModuleService.
  content::BrowserContext* context_;

  // Whether or not we are actively observing installs.
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(SharedModuleService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SHARED_MODULE_SERVICE_H_
