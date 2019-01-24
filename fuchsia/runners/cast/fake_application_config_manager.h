// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_
#define FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_

#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Test cast.ApplicationConfigManager implementation which maps a test Cast
// AppId to an embedded test server address.
class FakeApplicationConfigManager
    : public chromium::cast::ApplicationConfigManager {
 public:
  static const char kTestCastAppId[];

  explicit FakeApplicationConfigManager(
      net::EmbeddedTestServer* embedded_test_server);
  ~FakeApplicationConfigManager() override;

  // chromium::cast::ApplicationConfigManager interface.
  void GetConfig(std::string id, GetConfigCallback config_callback) override;

 private:
  net::EmbeddedTestServer* embedded_test_server_;

  DISALLOW_COPY_AND_ASSIGN(FakeApplicationConfigManager);
};

#endif  // FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_
