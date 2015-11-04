// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_

#include <string>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
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
  void SetNetworkState(
      const std::string& client_id,
      scoped_ptr<DevToolsNetworkConditions> conditions);

  base::WeakPtr<DevToolsNetworkInterceptor> GetInterceptor(
      const std::string& client_id);

 private:
  using InterceptorMap =
      base::ScopedPtrHashMap<std::string,
                             scoped_ptr<DevToolsNetworkInterceptor>>;

  scoped_ptr<DevToolsNetworkInterceptor> default_interceptor_;
  scoped_ptr<DevToolsNetworkInterceptor> appcache_interceptor_;
  InterceptorMap interceptors_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
