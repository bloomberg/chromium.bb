// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SERVICE_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service.h"
// Needed to keep gmock happy.
#include "chrome/browser/extensions/extension_sync_data.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockExtensionService : public ExtensionServiceInterface {
 public:
  MockExtensionService();
  virtual ~MockExtensionService();

  MOCK_CONST_METHOD0(extensions, const ExtensionList*());
  MOCK_CONST_METHOD0(disabled_extensions, const ExtensionList*());
  MOCK_METHOD0(pending_extension_manager, PendingExtensionManager*());
  MOCK_METHOD3(UpdateExtension, void(const std::string&,
                                     const FilePath&,
                                     const GURL&));
  MOCK_CONST_METHOD2(GetExtensionById,
                     const Extension*(const std::string&, bool));
  MOCK_METHOD3(UninstallExtension,
               bool(const std::string&, bool, std::string*));
  MOCK_CONST_METHOD1(IsExtensionEnabled, bool(const std::string&));
  MOCK_CONST_METHOD1(IsExternalExtensionUninstalled,
                     bool(const std::string&));
  MOCK_METHOD1(EnableExtension, void(const std::string&));
  MOCK_METHOD1(DisableExtension, void(const std::string&));
  MOCK_METHOD1(UpdateExtensionBlacklist,
               void(const std::vector<std::string>&));
  MOCK_METHOD0(CheckAdminBlacklist, void());
  MOCK_CONST_METHOD1(IsIncognitoEnabled, bool(const std::string&));
  MOCK_METHOD2(SetIsIncognitoEnabled, void(const std::string&, bool));
  MOCK_METHOD0(CheckForUpdatesSoon, void());
  MOCK_METHOD2(ProcessSyncData,
               void(const ExtensionSyncData&,
                    PendingExtensionInfo::ShouldAllowInstallPredicate
                        should_allow_install));
};

#endif  // CHROME_BROWSER_EXTENSIONS_MOCK_EXTENSION_SERVICE_H_
