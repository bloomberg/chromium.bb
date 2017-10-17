// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_THROTTLING_THROTTLING_NETWORK_CONTROLLER_H_
#define CONTENT_NETWORK_THROTTLING_THROTTLING_NETWORK_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"

namespace content {

class NetworkConditions;
class ThrottlingNetworkInterceptor;

// ThrottlingController manages interceptors identified by client id
// and their throttling conditions.
class CONTENT_EXPORT ThrottlingController {
 public:
  // Applies network emulation configuration.
  static void SetConditions(const std::string& client_id,
                            std::unique_ptr<NetworkConditions>);
  static void DisableThrottling(const std::string& client_id);

  static ThrottlingNetworkInterceptor* GetInterceptor(
      const std::string& client_id);

 private:
  ThrottlingController();
  ~ThrottlingController();

  ThrottlingNetworkInterceptor* FindInterceptor(const std::string& client_id);
  void SetNetworkConditions(const std::string& client_id,
                            std::unique_ptr<NetworkConditions> conditions);

  static ThrottlingController* instance_;

  using InterceptorMap =
      std::map<std::string, std::unique_ptr<ThrottlingNetworkInterceptor>>;

  InterceptorMap interceptors_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ThrottlingController);
};

}  // namespace content

#endif  // CONTENT_NETWORK_THROTTLING_THROTTLING_NETWORK_CONTROLLER_H_
