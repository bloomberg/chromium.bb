// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/engine_resources.h"

#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"

namespace chrome_cleaner {

bool IsSupportedEngine(Engine::Name engine) {
  return engine == Engine::TEST_ONLY;
}

std::string GetEngineName(Engine::Name engine) {
  return "Test";
}

std::string GetEngineVersion(Engine::Name engine) {
  return "0.1";
}

ProcessInformation::Process GetEngineProcessType(Engine::Name engine) {
  return ProcessInformation::TEST_SANDBOX;
}

ResultCode GetEngineDisconnectionErrorCode(Engine::Name engine) {
  return RESULT_CODE_TEST_ENGINE_SANDBOX_DISCONNECTED_TOO_SOON;
}

void InitializePUPDataWithCatalog(Engine::Name engine) {
  PUPData::InitializePUPData({&TestUwSCatalog::GetInstance()});
}

int GetProtectedFilesDigestResourceId() {
  return 0;
}

int GetUwEMatchersResourceID() {
  return 0;
}

std::unordered_map<base::string16, int> GetEmbeddedLibraryResourceIds(
    Engine::Name engine) {
  return {};
}

int GetLibrariesDigestResourcesId(Engine::Name engine) {
  return 0;
}

base::string16 GetTestStubFileName(Engine::Name engine) {
  return base::string16();
}

std::set<base::string16> GetLibrariesToLoad(Engine::Name engine) {
  return {};
}

std::unordered_map<base::string16, base::string16> GetLibraryTestReplacements(
    Engine::Name engine) {
  return {};
}

std::vector<base::string16> GetDLLNames(Engine::Name engine) {
  return {};
}

}  // namespace chrome_cleaner
