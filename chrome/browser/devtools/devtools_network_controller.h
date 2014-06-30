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
class DevToolsNetworkTransaction;

namespace test {
class DevToolsNetworkControllerHelper;
}

// DevToolsNetworkController tracks DevToolsNetworkTransactions.
class DevToolsNetworkController {

 public:
  DevToolsNetworkController();
  virtual ~DevToolsNetworkController();

  // Applies network emulation configuration.
  void SetNetworkState(
      const std::string& client_id,
      scoped_ptr<DevToolsNetworkConditions> conditions);

  base::WeakPtr<DevToolsNetworkInterceptor> GetInterceptor(
      DevToolsNetworkTransaction* transaction);

 protected:
  friend class test::DevToolsNetworkControllerHelper;

 private:
  // Controller must be constructed on IO thread.
  base::ThreadChecker thread_checker_;

  void SetNetworkStateOnIO(
      const std::string& client_id,
      scoped_ptr<DevToolsNetworkConditions> conditions);

  typedef scoped_ptr<DevToolsNetworkInterceptor> Interceptor;
  Interceptor default_interceptor_;
  Interceptor appcache_interceptor_;
  typedef base::ScopedPtrHashMap<std::string, DevToolsNetworkInterceptor>
      Interceptors;
  Interceptors interceptors_;

  base::WeakPtrFactory<DevToolsNetworkController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
