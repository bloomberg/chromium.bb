// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "sync/api/sync_error_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

TestExtensionService::~TestExtensionService() {}

const ExtensionSet* TestExtensionService::extensions() const {
  ADD_FAILURE();
  return NULL;
}

const ExtensionSet* TestExtensionService::disabled_extensions() const {
  ADD_FAILURE();
  return NULL;
}

extensions::PendingExtensionManager*
TestExtensionService::pending_extension_manager() {
  ADD_FAILURE();
  return NULL;
}

bool TestExtensionService::UpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    const GURL& download_url,
    extensions::CrxInstaller** out_crx_installer) {
  ADD_FAILURE();
  return false;
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

const Extension* TestExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  ADD_FAILURE();
  return NULL;
}

void TestExtensionService::FinishDelayedInstallation(
    const std::string& extension_id) {
  ADD_FAILURE();
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

void TestExtensionService::CheckManagementPolicy() {
  ADD_FAILURE();
}

void TestExtensionService::CheckForUpdatesSoon() {
  ADD_FAILURE();
}

syncer::SyncMergeResult TestExtensionService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  ADD_FAILURE();
  return syncer::SyncMergeResult(type);
}

void TestExtensionService::StopSyncing(syncer::ModelType type) {
  ADD_FAILURE();
}

syncer::SyncDataList TestExtensionService::GetAllSyncData(
    syncer::ModelType type) const {
  ADD_FAILURE();
  return syncer::SyncDataList();
}

syncer::SyncError TestExtensionService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  ADD_FAILURE();
  return syncer::SyncError();
}

bool TestExtensionService::is_ready() {
  ADD_FAILURE();
  return false;
}

base::SequencedTaskRunner* TestExtensionService::GetFileTaskRunner() {
  ADD_FAILURE();
  return NULL;
}

void TestExtensionService::AddExtension(const Extension* extension) {
  ADD_FAILURE();
}

void TestExtensionService::AddComponentExtension(const Extension* extension) {
  ADD_FAILURE();
}

void TestExtensionService::UnloadExtension(
    const std::string& extension_id,
    extension_misc::UnloadedExtensionReason reason) {
  ADD_FAILURE();
}

void TestExtensionService::SyncExtensionChangeIfNeeded(
    const Extension& extension) {
  ADD_FAILURE();
}
