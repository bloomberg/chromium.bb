// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service.h"

namespace extensions {
class CrxInstaller;
class Extension;
}

// Implemention of ExtensionServiceInterface with default
// implementations for methods that add failures.  You should subclass
// this and override the methods you care about.
class TestExtensionService : public ExtensionServiceInterface {
 public:
  virtual ~TestExtensionService();

  // ExtensionServiceInterface implementation.
  virtual const extensions::ExtensionSet* extensions() const override;
  virtual extensions::PendingExtensionManager*
      pending_extension_manager() override;

  virtual bool UpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      bool file_ownership_passed,
      extensions::CrxInstaller** out_crx_installer) override;
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const override;
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const override;
  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const override;
  virtual void FinishDelayedInstallation(
      const std::string& extension_id) override;
  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const override;

  virtual void CheckManagementPolicy() override;
  virtual void CheckForUpdatesSoon() override;

  virtual bool is_ready() override;

  virtual base::SequencedTaskRunner* GetFileTaskRunner() override;

  virtual void AddExtension(const extensions::Extension* extension) override;
  virtual void AddComponentExtension(
      const extensions::Extension* extension) override;

  virtual void UnloadExtension(
      const std::string& extension_id,
      extensions::UnloadedExtensionInfo::Reason reason) override;
  virtual void RemoveComponentExtension(const std::string & extension_id)
      override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
