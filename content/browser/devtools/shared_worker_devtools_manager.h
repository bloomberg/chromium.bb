// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/common/content_export.h"

class GURL;

namespace content {
class DevToolsAgentHost;

// SharedWorkerDevToolsManager is used instead of WorkerDevToolsManager when
// "enable-embedded-shared-worker" flag is set.
// This class lives on UI thread.
class CONTENT_EXPORT SharedWorkerDevToolsManager {
 public:
  typedef std::pair<int, int> WorkerId;
  class SharedWorkerDevToolsAgentHost;

  // Returns the SharedWorkerDevToolsManager singleton.
  static SharedWorkerDevToolsManager* GetInstance();

  DevToolsAgentHost* GetDevToolsAgentHostForWorker(int worker_process_id,
                                                   int worker_route_id);

  // Returns true when the worker must be paused on start.
  bool WorkerCreated(int worker_process_id,
                     int worker_route_id,
                     const SharedWorkerInstance& instance);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);
  void WorkerContextStarted(int worker_process_id, int worker_route_id);

 private:
  friend struct DefaultSingletonTraits<SharedWorkerDevToolsManager>;
  friend class SharedWorkerDevToolsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SharedWorkerDevToolsManagerTest, BasicTest);
  FRIEND_TEST_ALL_PREFIXES(SharedWorkerDevToolsManagerTest, AttachTest);

  enum WorkerState {
    WORKER_UNINSPECTED,
    WORKER_INSPECTED,
    WORKER_TERMINATED,
    WORKER_PAUSED,
  };

  class WorkerInfo {
   public:
    explicit WorkerInfo(const SharedWorkerInstance& instance)
        : instance_(instance), state_(WORKER_UNINSPECTED), agent_host_(NULL) {}

    const SharedWorkerInstance& instance() const { return instance_; }
    WorkerState state() { return state_; }
    void set_state(WorkerState new_state) { state_ = new_state; }
    SharedWorkerDevToolsAgentHost* agent_host() { return agent_host_; }
    void set_agent_host(SharedWorkerDevToolsAgentHost* agent_host) {
      agent_host_ = agent_host;
    }

   private:
    const SharedWorkerInstance instance_;
    WorkerState state_;
    SharedWorkerDevToolsAgentHost* agent_host_;
  };

  typedef base::ScopedPtrHashMap<WorkerId, WorkerInfo> WorkerInfoMap;

  SharedWorkerDevToolsManager();
  virtual ~SharedWorkerDevToolsManager();

  void RemoveInspectedWorkerData(SharedWorkerDevToolsAgentHost* agent_host);

  WorkerInfoMap::iterator FindExistingWorkerInfo(
      const SharedWorkerInstance& instance);

  // Resets to its initial state as if newly created.
  void ResetForTesting();

  WorkerInfoMap workers_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
