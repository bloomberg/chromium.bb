// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_DEVICE_TOKEN_FETCHER_H_
#define CHROME_TEST_TESTING_DEVICE_TOKEN_FETCHER_H_
#pragma once

#include <string>

#include "chrome/browser/policy/device_token_fetcher.h"

class DeviceTokenService;
class Profile;

namespace policy {

extern const char* kTestManagedDomainUsername;

// Replacement for DeviceTokenFetcher in tests. The only difference in internal
// logic is that the name of the currently logged in user is not fetched from
// external objects, but stored internally.
class TestingDeviceTokenFetcher
    : public DeviceTokenFetcher {
 public:
  TestingDeviceTokenFetcher(
      DeviceManagementBackend* backend,
      Profile* profile,
      const FilePath& token_path)
          : DeviceTokenFetcher(backend, profile, token_path) {}

  void SimulateLogin(const std::string& username);

 protected:
  virtual std::string GetCurrentUser();

 private:
  // This username will be reported as currently logged in.
  std::string username_;

  DISALLOW_COPY_AND_ASSIGN(TestingDeviceTokenFetcher);
};

}  // namespace policy

#endif  // CHROME_TEST_TESTING_DEVICE_TOKEN_FETCHER_H_
