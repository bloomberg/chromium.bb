// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service.h"

class CrxInstaller;

// Implemention of ExtensionServiceInterface with default
// implementations for methods that add failures.  You should subclass
// this and override the methods you care about.
class TestExtensionService : public ExtensionServiceInterface {
 public:
  virtual ~TestExtensionService();

  // ExtensionServiceInterface implementation.
  virtual const ExtensionList* extensions() const OVERRIDE;
  virtual PendingExtensionManager* pending_extension_manager() OVERRIDE;

  virtual bool UpdateExtension(
      const std::string& id,
      const FilePath& path,
      const GURL& download_url,
      CrxInstaller** out_crx_installer) OVERRIDE;
  virtual const Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;
  virtual const Extension* GetInstalledExtension(
      const std::string& id) const OVERRIDE;
  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const OVERRIDE;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const OVERRIDE;

  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) OVERRIDE;
  virtual void CheckAdminBlacklist() OVERRIDE;
  virtual void CheckForUpdatesSoon() OVERRIDE;
  virtual bool GetSyncData(
      const Extension& extension, ExtensionFilter filter,
      ExtensionSyncData* extension_sync_data) const OVERRIDE;
  virtual std::vector<ExtensionSyncData> GetSyncDataList(
      ExtensionFilter filter) const OVERRIDE;
  virtual void ProcessSyncData(
      const ExtensionSyncData& extension_sync_data,
      ExtensionFilter filter) OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SERVICE_H_
