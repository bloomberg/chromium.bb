// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_HANDLE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_HANDLE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

class DevToolsNetworkConditions;
class DevToolsNetworkController;

// A handle to manage an IO-thread DevToolsNetworkController on the IO thread
// while allowing SetNetworkState to be called from the UI thread. Must be
// created on the UI thread and destroyed on the IO thread.
class DevToolsNetworkControllerHandle {
 public:
  DevToolsNetworkControllerHandle();
  ~DevToolsNetworkControllerHandle();

  // Called on the UI thread.
  void SetNetworkState(const std::string& client_id,
                       scoped_ptr<DevToolsNetworkConditions> conditions);

  // Called on the IO thread.
  DevToolsNetworkController* GetController();

 private:
  void LazyInitialize();
  void SetNetworkStateOnIO(const std::string& client_id,
                           scoped_ptr<DevToolsNetworkConditions> conditions);

  scoped_ptr<DevToolsNetworkController> controller_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkControllerHandle);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_HANDLE_H_
