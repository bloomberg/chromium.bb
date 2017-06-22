// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SESSION_RUNNER_H_
#define COMPONENTS_ARC_ARC_SESSION_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/arc/arc_session.h"
#include "components/arc/arc_stop_reason.h"

namespace arc {

// Accept requests to start/stop ARC instance. Also supports automatic
// restarting on unexpected ARC instance crash.
class ArcSessionRunner : public ArcSession::Observer {
 public:
  // Observer to notify events across multiple ARC session runs.
  class Observer {
   public:
    // Called when ARC instance is stopped. If |restarting| is true, another
    // ARC session is being restarted (practically after certain delay).
    // Note: this is called once per ARC session, including unexpected
    // CRASH on ARC container, and expected SHUTDOWN of ARC triggered by
    // RequestStop(), so may be called multiple times for one RequestStart().
    virtual void OnSessionStopped(ArcStopReason reason, bool restarting) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // This is the factory interface to inject ArcSession instance
  // for testing purpose.
  using ArcSessionFactory = base::Callback<std::unique_ptr<ArcSession>()>;

  explicit ArcSessionRunner(const ArcSessionFactory& factory);
  ~ArcSessionRunner() override;

  // Add/Remove an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts the ARC service, then it will connect the Mojo channel. When the
  // bridge becomes ready, registered Observer's OnSessionReady() is called.
  void RequestStart();

  // Stops the ARC service.
  void RequestStop();

  // OnShutdown() should be called when the browser is shutting down. This can
  // only be called on the thread that this class was created on. We assume that
  // when this function is called, MessageLoop is no longer exists.
  void OnShutdown();

  // Returns whether currently ARC instance is running or stopped respectively.
  // Note that, both can return false at same time when, e.g., starting
  // or stopping ARC instance.
  bool IsRunning() const;
  bool IsStopped() const;

  // Returns the current ArcSession instance for testing purpose.
  ArcSession* GetArcSessionForTesting() { return arc_session_.get(); }

  // Normally, automatic restarting happens after a short delay. When testing,
  // however, we'd like it to happen immediately to avoid adding unnecessary
  // delays.
  void SetRestartDelayForTesting(const base::TimeDelta& restart_delay);

 private:
  // The possible states.  In the normal flow, the state changes in the
  // following sequence:
  //
  // STOPPED
  //   RequestStart() ->
  // STARTING
  //   OnSessionReady() ->
  // RUNNING
  //
  // The ArcSession state machine can be thought of being substates of
  // ArcBridgeService's STARTING state.
  // ArcBridgeService's state machine can be stopped at any phase.
  //
  // *
  //   RequestStop() ->
  // STOPPING
  //   OnSessionStopped() ->
  // STOPPED
  enum class State {
    // ARC instance is not currently running.
    STOPPED,

    // Request to start ARC instance is received. Starting an ARC instance.
    STARTING,

    // ARC instance has finished initializing, and is now ready for interaction
    // with other services.
    RUNNING,

    // Request to stop ARC instance is recieved. Stopping the ARC instance.
    STOPPING,
  };

  // Starts to run an ARC instance.
  void StartArcSession();

  // ArcSession::Observer:
  void OnSessionReady() override;
  void OnSessionStopped(ArcStopReason reason) override;

  THREAD_CHECKER(thread_checker_);

  // Observers for the ARC instance state change events.
  base::ObserverList<Observer> observer_list_;

  // Whether a client requests to run session or not.
  bool run_requested_ = false;

  // Instead of immediately trying to restart the container, give it some time
  // to finish tearing down in case it is still in the process of stopping.
  base::TimeDelta restart_delay_;
  base::OneShotTimer restart_timer_;

  // Factory to inject a fake ArcSession instance for testing.
  ArcSessionFactory factory_;

  // Current runner's state.
  State state_ = State::STOPPED;

  // ArcSession object for currently running ARC instance. This should be
  // nullptr if the state is STOPPED, otherwise non-nullptr.
  std::unique_ptr<ArcSession> arc_session_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcSessionRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionRunner);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SESSION_RUNNER_H_
