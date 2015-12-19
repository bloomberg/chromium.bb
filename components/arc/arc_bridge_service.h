// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/arc/common/arc_bridge.mojom.h"

namespace base {
class CommandLine;
}  // namespace base

namespace arc {

class ArcBridgeBootstrap;

// The Chrome-side service that handles ARC instances and ARC bridge creation.
// This service handles the lifetime of ARC instances and sets up the
// communication channel (the ARC bridge) used to send and receive messages.
class ArcBridgeService : public ArcBridgeHost {
 public:
  // The possible states of the bridge.  In the normal flow, the state changes
  // in the following sequence:
  //
  // STOPPED
  //   PrerequisitesChanged() ->
  // CONNECTING
  //   OnConnectionEstablished() ->
  // READY
  //
  // The ArcBridgeBootstrap state machine can be thought of being substates of
  // ArcBridgeService's CONNECTING state.
  //
  // *
  //   StopInstance() ->
  // STOPPING
  //   OnStopped() ->
  // STOPPED
  enum class State {
    // ARC is not currently running.
    STOPPED,

    // The request to connect has been sent.
    CONNECTING,

    // The instance has started, and the bridge is fully established.
    CONNECTED,

    // The ARC instance has finished initializing and is now ready for user
    // interaction.
    READY,

    // The ARC instance has started shutting down.
    STOPPING,
  };

  // Notifies life cycle events of ArcBridgeService.
  class Observer {
   public:
    // Called whenever the state of the bridge has changed.
    virtual void OnStateChanged(State state) {}

    // Called whenever ARC's availability has changed for this system.
    virtual void OnAvailableChanged(bool available) {}

    // Called whenever the ARC app list is ready.
    virtual void OnAppInstanceReady() {}

    // Called whenever the ARC input is ready.
    virtual void OnInputInstanceReady() {}

    // Called whenever the ARC notification is ready.
    virtual void OnNotificationsInstanceReady() {}

    // Called whenever the ARC power is ready.
    virtual void OnPowerInstanceReady() {}

    // Called whenever the ARC process is ready.
    virtual void OnProcessInstanceReady() {}

    // Called whenever the ARC settings is ready.
    virtual void OnSettingsInstanceReady() {}

   protected:
    virtual ~Observer() {}
  };

  ~ArcBridgeService() override;

  // Gets the global instance of the ARC Bridge Service. This can only be
  // called on the thread that this class was created on.
  static ArcBridgeService* Get();

  // Return true if ARC has been enabled through a commandline
  // switch.
  static bool GetEnabled(const base::CommandLine* command_line);

  // DetectAvailability() should be called once D-Bus is available. It will
  // call CheckArcAvailability() on the session_manager. This can only be
  // called on the thread that this class was created on.
  virtual void DetectAvailability() = 0;

  // HandleStartup() should be called upon profile startup.  This will only
  // launch an instance if the instance service is available and it is enabled.
  // This can only be called on the thread that this class was created on.
  virtual void HandleStartup() = 0;

  // Shutdown() should be called when the browser is shutting down. This can
  // only be called on the thread that this class was created on.
  virtual void Shutdown() = 0;

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the Mojo interface for all the instance services. This will return
  // nullptr if that particular service is not ready yet. Use an Observer if
  // you want to be notified when this is ready. This can only be called on the
  // thread that this class was created on.
  AppInstance* app_instance() { return app_ptr_.get(); }
  InputInstance* input_instance() { return input_ptr_.get(); }
  NotificationsInstance* notifications_instance() {
    return notifications_ptr_.get();
  }
  PowerInstance* power_instance() { return power_ptr_.get(); }
  ProcessInstance* process_instance() { return process_ptr_.get(); }
  SettingsInstance* settings_instance() { return settings_ptr_.get(); }

  // ArcHost:
  void OnAppInstanceReady(AppInstancePtr app_ptr) override;
  void OnInputInstanceReady(InputInstancePtr input_ptr) override;
  void OnNotificationsInstanceReady(
      NotificationsInstancePtr notifications_ptr) override;
  void OnPowerInstanceReady(PowerInstancePtr power_ptr) override;
  void OnProcessInstanceReady(ProcessInstancePtr process_ptr) override;
  void OnSettingsInstanceReady(SettingsInstancePtr process_ptr) override;

  // Gets the current state of the bridge service.
  State state() const { return state_; }

  // Gets if ARC is available in this system.
  bool available() const { return available_; }

 protected:
  ArcBridgeService();

  // Changes the current state and notifies all observers.
  void SetState(State state);

  // Changes the current availability and notifies all observers.
  void SetAvailable(bool availability);

  base::ObserverList<Observer>& observer_list() { return observer_list_; }

  bool CalledOnValidThread();

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Prerequisites);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, ShutdownMidStartup);

  // Mojo interfaces.
  AppInstancePtr app_ptr_;
  InputInstancePtr input_ptr_;
  NotificationsInstancePtr notifications_ptr_;
  PowerInstancePtr power_ptr_;
  ProcessInstancePtr process_ptr_;
  SettingsInstancePtr settings_ptr_;

  base::ObserverList<Observer> observer_list_;

  base::ThreadChecker thread_checker_;

  // If the ARC instance service is available.
  bool available_;

  // The current state of the bridge.
  ArcBridgeService::State state_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
