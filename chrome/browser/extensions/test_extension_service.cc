// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TestExtensionService::~TestExtensionService() {}

const ExtensionList* TestExtensionService::extensions() const {
  ADD_FAILURE();
  return NULL;
}

PendingExtensionManager* TestExtensionService::pending_extension_manager() {
  ADD_FAILURE();
  return NULL;
}

bool TestExtensionService::UpdateExtension(
    const std::string& id,
    const FilePath& path,
    const GURL& download_url,
    CrxInstaller** out_crx_installer) {
  ADD_FAILURE();
  return NULL;
}

const Extension* TestExtensionService::GetExtensionById(
    const std::string& id, bool include_disabled) const {
  ADD_FAILURE();
  return NULL;
}

const Extension* TestExtensionService::GetInstalledExtension(
    const std::string& id) const {
  ADD_FAILURE();
  return NULL;
}

bool TestExtensionService::IsExtensionEnabled(
    const std::string& extension_id) const {
  ADD_FAILURE();
  return false;
}

bool TestExtensionService::IsExternalExtensionUninstalled(
    const std::string& extension_id) const {
  ADD_FAILURE();
  return false;
}

void TestExtensionService::UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) {
  ADD_FAILURE();
}

void TestExtensionService::CheckAdminBlacklist() {
  ADD_FAILURE();
}

void TestExtensionService::CheckForUpdatesSoon() {
  ADD_FAILURE();
}

bool TestExtensionService::GetSyncData(
    const Extension& extension, ExtensionFilter filter,
    ExtensionSyncData* extension_sync_data) const {
  ADD_FAILURE();
  return false;
}

std::vector<ExtensionSyncData> TestExtensionService::GetSyncDataList(
    ExtensionFilter filter) const {
  ADD_FAILURE();
  return std::vector<ExtensionSyncData>();
}

void TestExtensionService::ProcessSyncData(
    const ExtensionSyncData& extension_sync_data,
    ExtensionFilter filter) {
  ADD_FAILURE();
}
