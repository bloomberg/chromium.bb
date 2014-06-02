// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

class DevToolsNetworkTransaction;
class GURL;
class Profile;

namespace content {
class ResourceContext;
}

namespace net {
struct HttpRequestInfo;
}

namespace test {
class DevToolsNetworkControllerHelper;
}

// DevToolsNetworkController tracks DevToolsNetworkTransactions.
class DevToolsNetworkController {

 public:
  DevToolsNetworkController();
  virtual ~DevToolsNetworkController();

  void AddTransaction(DevToolsNetworkTransaction* transaction);

  void RemoveTransaction(DevToolsNetworkTransaction* transaction);

  // Applies network emulation configuration.
  // |client_id| should be DevToolsAgentHost GUID.
  void SetNetworkState(
      const std::string& client_id,
      bool disable_network);

  bool ShouldFail(const net::HttpRequestInfo* request);

 protected:
  friend class test::DevToolsNetworkControllerHelper;

 private:
  // Controller must be constructed on IO thread.
  base::ThreadChecker thread_checker_;

  void SetNetworkStateOnIO(
      const std::string& client_id,
      bool disable_network);

  typedef std::set<DevToolsNetworkTransaction*> Transactions;
  Transactions transactions_;

  typedef std::set<std::string> Clients;
  Clients clients_;

  base::WeakPtrFactory<DevToolsNetworkController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONTROLLER_H_
