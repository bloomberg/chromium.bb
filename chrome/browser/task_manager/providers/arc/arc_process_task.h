// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/common/process.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

namespace task_manager {

// Defines a task that represents an ARC process.
class ArcProcessTask
    : public Task,
      public arc::ConnectionObserver<arc::mojom::IntentHelperInstance> {
 public:
  ArcProcessTask(base::ProcessId pid,
                 base::ProcessId nspid,
                 const std::string& process_name,
                 arc::mojom::ProcessState process_state,
                 const std::vector<std::string>& packages);
  ~ArcProcessTask() override;

  // task_manager::Task:
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;
  bool IsKillable() override;
  void Kill() override;

  // arc::ConnectionObserver<arc::mojom::IntentHelperInstance>:
  void OnConnectionReady() override;

  void SetProcessState(arc::mojom::ProcessState process_state);

  base::ProcessId nspid() const { return nspid_; }
  const std::string& process_name() const { return process_name_; }

 private:
  void StartIconLoading();
  void OnIconLoaded(
      std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

  const base::ProcessId nspid_;
  const std::string process_name_;
  arc::mojom::ProcessState process_state_;
  const std::string package_name_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcProcessTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcessTask);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_H_
