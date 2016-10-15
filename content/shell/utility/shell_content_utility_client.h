// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_SHELL_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_

#include "content/public/utility/content_utility_client.h"

namespace content {

class ShellContentUtilityClient : public ContentUtilityClient {
 public:
  ~ShellContentUtilityClient() override;

  // ContentUtilityClient:
  void RegisterServices(StaticServiceMap* services) override;
  void ExposeInterfacesToBrowser(
      service_manager::InterfaceRegistry* registry) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_
