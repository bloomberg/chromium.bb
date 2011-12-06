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

SyncError TestExtensionService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  ADD_FAILURE();
  return SyncError();
}

void TestExtensionService::StopSyncing(syncable::ModelType type) {
  ADD_FAILURE();
}

SyncDataList TestExtensionService::GetAllSyncData(
    syncable::ModelType type) const {
  ADD_FAILURE();
  return SyncDataList();
}

SyncError TestExtensionService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  ADD_FAILURE();
  return SyncError();
}

bool TestExtensionService::is_ready() {
  ADD_FAILURE();
  return false;
}

void TestExtensionService::AddExtension(const Extension* extension) {
  ADD_FAILURE();
}

void TestExtensionService::UnloadExtension(
    const std::string& extension_id,
    extension_misc::UnloadedExtensionReason reason) {
  ADD_FAILURE();
}
