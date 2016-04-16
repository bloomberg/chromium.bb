// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_SERVICE_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"

namespace arc {

// A single global entry to get a list of ARC processes.
//
// Call RequestProcessList() on the main UI thread to get a list of all ARC
// processes. It returns vector<arc::ArcProcess>, which includes pid <-> nspid
// mapping.
// Example:
//   void OnUpdateProcessList(const vector<arc::ArcProcess>&) {...}
//
//   arc::ArcProcessService* arc_process_service =
//       arc::ArcProcessService::Get();
//   if (!arc_process_service ||
//       !arc_process_service->RequestProcessList(
//           base::Bind(&OnUpdateProcessList)) {
//     LOG(ERROR) << "ARC process instance not ready.";
//   }
class ArcProcessService : public ArcService,
                          public ArcBridgeService::Observer {
 public:
  using RequestProcessListCallback =
      base::Callback<void(const std::vector<ArcProcess>&)>;

  explicit ArcProcessService(ArcBridgeService* bridge_service);
  ~ArcProcessService() override;

  // Returns nullptr before the global instance is ready.
  static ArcProcessService* Get();

  // ArcBridgeService::Observer overrides.
  void OnProcessInstanceReady() override;

  // Returns true if ARC IPC is ready for process list request,
  // otherwise false.
  bool RequestProcessList(RequestProcessListCallback callback);

 private:
  void Reset();

  void OnReceiveProcessList(
      const RequestProcessListCallback& callback,
      mojo::Array<arc::mojom::RunningAppProcessInfoPtr> mojo_processes);

  void CallbackRelay(
      const RequestProcessListCallback& callback,
      const std::vector<ArcProcess>* ret_processes);

  void UpdateAndReturnProcessList(
      const std::vector<arc::mojom::RunningAppProcessInfoPtr>* raw_processes,
      std::vector<ArcProcess>* ret_processes);

  void PopulateProcessList(
      const std::vector<arc::mojom::RunningAppProcessInfoPtr>* raw_processes,
      std::vector<ArcProcess>* ret_processes);

  void UpdateNspidToPidMap();

  // Keep a cache pid mapping of all arc processes so to minimize the number of
  // nspid lookup from /proc/<PID>/status.
  // To play safe, always modify |nspid_to_pid_| on the worker thread.
  std::map<base::ProcessId, base::ProcessId> nspid_to_pid_;

  scoped_refptr<base::SequencedWorkerPool> worker_pool_;

  // To ensure internal state changes are done on the same worker thread.
  base::ThreadChecker thread_checker_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcProcessService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcessService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_SERVICE_H_
