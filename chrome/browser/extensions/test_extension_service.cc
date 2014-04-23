// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

TestExtensionService::~TestExtensionService() {}

const extensions::ExtensionSet* TestExtensionService::extensions() const {
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
    bool file_ownership_passed,
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

void TestExtensionService::CheckManagementPolicy() {
  ADD_FAILURE();
}

void TestExtensionService::CheckForUpdatesSoon() {
  ADD_FAILURE();
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
    extensions::UnloadedExtensionInfo::Reason reason) {
  ADD_FAILURE();
}

void TestExtensionService::RemoveComponentExtension(
    const std::string& extension_id) {
  ADD_FAILURE();
}
