// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
#define CONTENT_COMMON_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsNetworkConditions;
class DevToolsNetworkInterceptor;

// DevToolsNetworkController manages interceptors identified by client id
// and their throttling conditions.
class CONTENT_EXPORT DevToolsNetworkController {
 public:
  // Applies network emulation configuration.
  static void SetNetworkState(
      const std::string& client_id,
      std::unique_ptr<DevToolsNetworkConditions> conditions);

  static DevToolsNetworkInterceptor* GetInterceptor(
      const std::string& client_id);

 private:
  DevToolsNetworkController();
  ~DevToolsNetworkController();

  DevToolsNetworkInterceptor* FindInterceptor(const std::string& client_id);
  void SetConditions(const std::string& client_id,
                     std::unique_ptr<DevToolsNetworkConditions> conditions);

  static DevToolsNetworkController* instance_;

  using InterceptorMap =
      std::map<std::string, std::unique_ptr<DevToolsNetworkInterceptor>>;

  InterceptorMap interceptors_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkController);
};

}  // namespace content

#endif  // CONTENT_COMMON_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
