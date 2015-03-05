// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "content/browser/devtools/worker_devtools_manager.h"

namespace content {

class SharedWorkerDevToolsAgentHost;
class SharedWorkerInstance;

// Manages WorkerDevToolsAgentHost's for Shared Workers.
// This class lives on UI thread.
class CONTENT_EXPORT SharedWorkerDevToolsManager
    : public WorkerDevToolsManager {
 public:
  // Returns the SharedWorkerDevToolsManager singleton.
  static SharedWorkerDevToolsManager* GetInstance();

  // Returns true when the worker must be paused on start because a DevTool
  // window for the same former SharedWorkerInstance is still opened.
  bool WorkerCreated(int worker_process_id,
                     int worker_route_id,
                     const SharedWorkerInstance& instance);

 private:
  friend struct DefaultSingletonTraits<SharedWorkerDevToolsManager>;
  friend class SharedWorkerDevToolsAgentHost;
  friend class SharedWorkerDevToolsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SharedWorkerDevToolsManagerTest, BasicTest);
  FRIEND_TEST_ALL_PREFIXES(SharedWorkerDevToolsManagerTest, AttachTest);

  SharedWorkerDevToolsManager();
  ~SharedWorkerDevToolsManager() override;

  AgentHostMap::iterator FindExistingWorkerAgentHost(
      const SharedWorkerInstance& instance);

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
