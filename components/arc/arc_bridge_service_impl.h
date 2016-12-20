// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_session_observer.h"

template <typename T>
class scoped_refptr;

namespace base {
class TaskRunner;
}  // namespace base

namespace arc {

class ArcSession;

// Real IPC based ArcBridgeService that is used in production.
class ArcBridgeServiceImpl : public ArcBridgeService,
                             public ArcSessionObserver {
 public:
  // This is the factory interface to inject ArcSession instance
  // for testing purpose.
  using ArcSessionFactory = base::Callback<std::unique_ptr<ArcSession>()>;

  explicit ArcBridgeServiceImpl(
      const scoped_refptr<base::TaskRunner>& blocking_task_runner);
  ~ArcBridgeServiceImpl() override;

  // ArcBridgeService overrides:
  void RequestStart() override;
  void RequestStop() override;
  void OnShutdown() override;

  // Inject a factory to create ArcSession instance for testing purpose.
  // |factory| must not be null.
  void SetArcSessionFactoryForTesting(const ArcSessionFactory& factory);

  // Returns the current ArcSession instance for testing purpose.
  ArcSession* GetArcSessionForTesting() { return arc_session_.get(); }

  // Normally, automatic restarting happens after a short delay. When testing,
  // however, we'd like it to happen immediately to avoid adding unnecessary
  // delays.
  void SetRestartDelayForTesting(const base::TimeDelta& restart_delay);

 private:
  // Starts to run an ARC instance.
  void StartArcSession();

  // Stops the running instance.
  void StopInstance();

  // ArcSessionObserver:
  void OnSessionReady() override;
  void OnSessionStopped(StopReason reason) override;

  // Whether a client requests to run session or not.
  bool run_requested_ = false;

  // Instead of immediately trying to restart the container, give it some time
  // to finish tearing down in case it is still in the process of stopping.
  base::TimeDelta restart_delay_;
  base::OneShotTimer restart_timer_;

  // Factory to inject a fake ArcSession instance for testing.
  ArcSessionFactory factory_;

  // ArcSession object for currently running ARC instance. This should be
  // nullptr if the state is STOPPED, otherwise non-nullptr.
  std::unique_ptr<ArcSession> arc_session_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
