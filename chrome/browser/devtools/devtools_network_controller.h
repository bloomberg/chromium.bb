// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/threading/thread_checker.h"

class DevToolsNetworkConditions;
class DevToolsNetworkInterceptor;

// DevToolsNetworkController manages interceptors identified by client id
// and their throttling conditions.
class DevToolsNetworkController {
 public:
  DevToolsNetworkController();
  virtual ~DevToolsNetworkController();

  // Applies network emulation configuration.
  void SetNetworkState(const std::string& client_id,
                       std::unique_ptr<DevToolsNetworkConditions> conditions);

  DevToolsNetworkInterceptor* GetInterceptor(
      const std::string& client_id);

 private:
  using InterceptorMap =
      std::unordered_map<std::string,
                         std::unique_ptr<DevToolsNetworkInterceptor>>;

  std::unique_ptr<DevToolsNetworkInterceptor> appcache_interceptor_;
  InterceptorMap interceptors_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
