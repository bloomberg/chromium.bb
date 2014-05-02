// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_MANAGER_H_

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

// EmbeddedWorkerDevToolsManager is used instead of WorkerDevToolsManager when
// "enable-embedded-shared-worker" flag is set.
// This class lives on UI thread.
class CONTENT_EXPORT EmbeddedWorkerDevToolsManager {
 public:
  typedef std::pair<int, int> WorkerId;
  class EmbeddedWorkerDevToolsAgentHost;

  // Returns the EmbeddedWorkerDevToolsManager singleton.
  static EmbeddedWorkerDevToolsManager* GetInstance();

  DevToolsAgentHost* GetDevToolsAgentHostForWorker(int worker_process_id,
                                                   int worker_route_id);

  // Returns true when the worker must be paused on start.
  bool SharedWorkerCreated(int worker_process_id,
                           int worker_route_id,
                           const SharedWorkerInstance& instance);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);
  void WorkerContextStarted(int worker_process_id, int worker_route_id);

 private:
  friend struct DefaultSingletonTraits<EmbeddedWorkerDevToolsManager>;
  friend class EmbeddedWorkerDevToolsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerDevToolsManagerTest, BasicTest);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerDevToolsManagerTest, AttachTest);

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
    EmbeddedWorkerDevToolsAgentHost* agent_host() { return agent_host_; }
    void set_agent_host(EmbeddedWorkerDevToolsAgentHost* agent_host) {
      agent_host_ = agent_host;
    }

   private:
    const SharedWorkerInstance instance_;
    WorkerState state_;
    EmbeddedWorkerDevToolsAgentHost* agent_host_;
  };

  typedef base::ScopedPtrHashMap<WorkerId, WorkerInfo> WorkerInfoMap;

  EmbeddedWorkerDevToolsManager();
  virtual ~EmbeddedWorkerDevToolsManager();

  void RemoveInspectedWorkerData(EmbeddedWorkerDevToolsAgentHost* agent_host);

  WorkerInfoMap::iterator FindExistingSharedWorkerInfo(
      const SharedWorkerInstance& instance);

  // Resets to its initial state as if newly created.
  void ResetForTesting();

  WorkerInfoMap workers_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_MANAGER_H_
