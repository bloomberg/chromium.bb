// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_SHELL_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_

#include "base/macros.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/utility/content_utility_client.h"

namespace content {

class ShellContentUtilityClient : public ContentUtilityClient {
 public:
  ShellContentUtilityClient();
  ~ShellContentUtilityClient() override;

  // ContentUtilityClient:
  void UtilityThreadStarted() override;
  void RegisterServices(StaticServiceMap* services) override;
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override;

 private:
  NetworkServiceTestHelper network_service_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ShellContentUtilityClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_
