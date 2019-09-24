// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

TestWebAppRegistryController::TestWebAppRegistryController() = default;

TestWebAppRegistryController::~TestWebAppRegistryController() = default;

void TestWebAppRegistryController::SetUp(Profile* profile) {
  database_factory_ = std::make_unique<TestWebAppDatabaseFactory>();
  registrar_ = std::make_unique<WebAppRegistrar>(profile);

  sync_bridge_ = std::make_unique<WebAppSyncBridge>(
      profile, database_factory_.get(), registrar_.get(),
      mock_processor_.CreateForwardingProcessor());

  ON_CALL(processor(), IsTrackingMetadata())
      .WillByDefault(testing::Return(true));
}

void TestWebAppRegistryController::Init() {
  base::RunLoop run_loop;
  sync_bridge_->Init(run_loop.QuitClosure());
  run_loop.Run();
}

void TestWebAppRegistryController::DestroySubsystems() {
  registrar_.reset();
  sync_bridge_.reset();
  database_factory_.reset();
}

}  // namespace web_app
