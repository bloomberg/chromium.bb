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

    // Called whenever the ARC app interface state changes.
    virtual void OnAppInstanceReady() {}
    virtual void OnAppInstanceClosed() {}

    // Called whenever the ARC auth interface state changes.
    virtual void OnAuthInstanceReady() {}
    virtual void OnAuthInstanceClosed() {}

    // Called whenever the ARC clipboard interface state changes.
    virtual void OnClipboardInstanceReady() {}
    virtual void OnClipboardInstanceClosed() {}

    // Called whenever the ARC IME interface state changes.
    virtual void OnImeInstanceReady() {}
    virtual void OnImeInstanceClosed() {}

    // Called whenever the ARC input interface state changes.
    virtual void OnInputInstanceReady() {}
    virtual void OnInputInstanceClosed() {}

    // Called whenever the ARC intent helper interface state changes.
    virtual void OnIntentHelperInstanceReady() {}
    virtual void OnIntentHelperInstanceClosed() {}

    // Called whenever the ARC notification interface state changes.
    virtual void OnNotificationsInstanceReady() {}
    virtual void OnNotificationsInstanceClosed() {}

    // Called whenever the ARC power interface state changes.
    virtual void OnPowerInstanceReady() {}
    virtual void OnPowerInstanceClosed() {}

    // Called whenever the ARC process interface state changes.
    virtual void OnProcessInstanceReady() {}
    virtual void OnProcessInstanceClosed() {}

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
  AuthInstance* auth_instance() { return auth_ptr_.get(); }
  ClipboardInstance* clipboard_instance() { return clipboard_ptr_.get(); }
  ImeInstance* ime_instance() { return ime_ptr_.get(); }
  InputInstance* input_instance() { return input_ptr_.get(); }
  IntentHelperInstance* intent_helper_instance() {
    return intent_helper_ptr_.get();
  }
  NotificationsInstance* notifications_instance() {
    return notifications_ptr_.get();
  }
  PowerInstance* power_instance() { return power_ptr_.get(); }
  ProcessInstance* process_instance() { return process_ptr_.get(); }

  int32_t app_version() const { return app_ptr_.version(); }
  int32_t auth_version() const { return auth_ptr_.version(); }
  int32_t clipboard_version() const { return clipboard_ptr_.version(); }
  int32_t ime_version() const { return ime_ptr_.version(); }
  int32_t input_version() const { return input_ptr_.version(); }
  int32_t intent_helper_version() const { return intent_helper_ptr_.version(); }
  int32_t notifications_version() const { return notifications_ptr_.version(); }
  int32_t power_version() const { return power_ptr_.version(); }
  int32_t process_version() const { return process_ptr_.version(); }

  // ArcHost:
  void OnAppInstanceReady(AppInstancePtr app_ptr) override;
  void OnAuthInstanceReady(AuthInstancePtr auth_ptr) override;
  void OnClipboardInstanceReady(ClipboardInstancePtr clipboard_ptr) override;
  void OnImeInstanceReady(ImeInstancePtr ime_ptr) override;
  void OnInputInstanceReady(InputInstancePtr input_ptr) override;
  void OnIntentHelperInstanceReady(
      IntentHelperInstancePtr intent_helper_ptr) override;
  void OnNotificationsInstanceReady(
      NotificationsInstancePtr notifications_ptr) override;
  void OnPowerInstanceReady(PowerInstancePtr power_ptr) override;
  void OnProcessInstanceReady(ProcessInstancePtr process_ptr) override;
  void OnSettingsInstanceReady(SettingsInstancePtr settings_ptr) override;

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

  // Closes all Mojo channels.
  void CloseAllChannels();

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Prerequisites);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, ShutdownMidStartup);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Restart);

  // Called when one of the individual channels is closed.
  void CloseAppChannel();
  void CloseAuthChannel();
  void CloseClipboardChannel();
  void CloseImeChannel();
  void CloseInputChannel();
  void CloseIntentHelperChannel();
  void CloseNotificationsChannel();
  void ClosePowerChannel();
  void CloseProcessChannel();

  // Callbacks for QueryVersion.
  void OnAppVersionReady(int32_t version);
  void OnAuthVersionReady(int32_t version);
  void OnClipboardVersionReady(int32_t version);
  void OnImeVersionReady(int32_t version);
  void OnInputVersionReady(int32_t version);
  void OnIntentHelperVersionReady(int32_t version);
  void OnNotificationsVersionReady(int32_t version);
  void OnPowerVersionReady(int32_t version);
  void OnProcessVersionReady(int32_t version);

  // Mojo interfaces.
  AppInstancePtr app_ptr_;
  AuthInstancePtr auth_ptr_;
  ClipboardInstancePtr clipboard_ptr_;
  ImeInstancePtr ime_ptr_;
  InputInstancePtr input_ptr_;
  IntentHelperInstancePtr intent_helper_ptr_;
  NotificationsInstancePtr notifications_ptr_;
  PowerInstancePtr power_ptr_;
  ProcessInstancePtr process_ptr_;

  // Temporary Mojo interfaces.  After a Mojo interface pointer has been
  // received from the other endpoint, we still need to asynchronously query
  // its version.  While that is going on, we should still return nullptr on
  // the xxx_instance() functions.
  // To keep the xxx_instance() functions being trivial, store the instance
  // pointer in a temporary variable to avoid losing its reference.
  AppInstancePtr temporary_app_ptr_;
  AuthInstancePtr temporary_auth_ptr_;
  ClipboardInstancePtr temporary_clipboard_ptr_;
  ImeInstancePtr temporary_ime_ptr_;
  InputInstancePtr temporary_input_ptr_;
  IntentHelperInstancePtr temporary_intent_helper_ptr_;
  NotificationsInstancePtr temporary_notifications_ptr_;
  PowerInstancePtr temporary_power_ptr_;
  ProcessInstancePtr temporary_process_ptr_;

  base::ObserverList<Observer> observer_list_;

  base::ThreadChecker thread_checker_;

  // If the ARC instance service is available.
  bool available_;

  // The current state of the bridge.
  ArcBridgeService::State state_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
