// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/fake_application_config_manager.h"

#include "base/logging.h"

const char FakeApplicationConfigManager::kTestCastAppId[] = "00000000";

FakeApplicationConfigManager::FakeApplicationConfigManager(
    net::EmbeddedTestServer* embedded_test_server)
    : embedded_test_server_(embedded_test_server) {}
FakeApplicationConfigManager::~FakeApplicationConfigManager() = default;

void FakeApplicationConfigManager::GetConfig(std::string id,
                                             GetConfigCallback callback) {
  if (id != kTestCastAppId) {
    LOG(ERROR) << "Unknown Cast app Id: " << id;
    callback(chromium::cast::ApplicationConfigPtr());
    return;
  }

  chromium::cast::ApplicationConfigPtr app_config =
      chromium::cast::ApplicationConfig::New();
  app_config->id = id;
  app_config->display_name = "Dummy test app";
  app_config->web_url = embedded_test_server_->base_url().spec();
  callback(std::move(app_config));
}
