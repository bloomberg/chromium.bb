// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace arc {

// The Chrome-side service that handles ARC instances and ARC bridge creation.
// This service handles the lifetime of ARC instances and sets up the
// communication channel (the ARC bridge) used to send and receive messages.
class ArcBridgeService : public IPC::Listener {
 public:
  // The possible states of the bridge.  In the normal flow, the state changes
  // in the following sequence:
  //
  // STOPPED
  //   SetAvailable(true) + HandleStartup() -> SocketConnect() ->
  // CONNECTING
  //   Connect() ->
  // CONNECTED
  //   SocketConnectAfterSetSocketPermissions() ->
  // STARTING
  //   StartInstance() -> OnInstanceReady() ->
  // READY
  //
  // When Shutdown() is called, the state changes depending on the state it was
  // at:
  //
  // CONNECTED/CONNECTING -> STOPPED
  // STARTING/READY -> STOPPING -> StopInstance() -> STOPPED
  enum class State {
    // ARC is not currently running.
    STOPPED,

    // The request to connect has been sent.
    CONNECTING,

    // The bridge has connected to the socket, but has not started the ARC
    // instance.
    CONNECTED,

    // The ARC bridge has been set up and ARC is starting up.
    STARTING,

    // The ARC instance has been fully initialized and is now ready for user
    // interaction.
    READY,

    // The ARC instance has started shutting down.
    STOPPING,
  };

  class Observer {
   public:
    // Called whenever the state of the bridge has changed.
    virtual void OnStateChanged(State state) {}

    // Called whenever ARC's availability has changed for this system.
    virtual void OnAvailableChanged(bool available) {}

   protected:
    virtual ~Observer() {}
  };

  ArcBridgeService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner);
  ~ArcBridgeService() override;

  // Gets the global instance of the ARC Bridge Service. This can only be
  // called on the thread that this class was created on.
  static ArcBridgeService* Get();

  // DetectAvailability() should be called once D-Bus is available. It will
  // call CheckArcAvailability() on the session_manager. This can only be
  // called on the thread that this class was created on.
  void DetectAvailability();

  // HandleStartup() should be called upon profile startup.  This will only
  // launch an instance if the instance service is available and it is enabled.
  // This can only be called on the thread that this class was created on.
  void HandleStartup();

  // Shutdown() should be called when the browser is shutting down. This can
  // only be called on the thread that this class was created on.
  void Shutdown();

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the current state of the bridge service.
  State state() const { return state_; }

  // Gets if ARC is available in this system.
  bool available() const { return available_; }

  // Requests registration of an input device on the ARC instance.
  // TODO(denniskempin): Make this interface more typesafe.
  // |name| should be the displayable name of the emulated device (e.g. "Chrome
  // OS Keyboard"), |device_type| the name of the device type (e.g. "keyboard")
  // and |fd| a file descriptor that emulates the kernel events of the device.
  // This can only be called on the thread that this class was created on.
  bool RegisterInputDevice(const std::string& name,
                           const std::string& device_type,
                           base::ScopedFD fd);

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, ShutdownMidStartup);

  // If all pre-requisites are true (ARC is available, it has been enabled, and
  // the session has started), and ARC is stopped, start ARC. If ARC is running
  // and the pre-requisites stop being true, stop ARC.
  void PrerequisitesChanged();

  // Binds to the socket specified by |socket_path|.
  void SocketConnect(const base::FilePath& socket_path);

  // Binds to the socket specified by |socket_path| after creating its parent
  // directory is present.
  void SocketConnectAfterEnsureParentDirectory(
      const base::FilePath& socket_path,
      bool directory_present);

  // Internal connection method. Separated to make testing easier.
  bool Connect(const IPC::ChannelHandle& handle, IPC::Channel::Mode mode);

  // Finishes connecting after setting socket permissions.
  void SocketConnectAfterSetSocketPermissions(const base::FilePath& socket_path,
                                              bool socket_permissions_success);

  // Stops the running instance.
  void StopInstance();

  // Called when the ARC instance has finished setting up and is ready for user
  // interaction.
  void OnInstanceReady();

  // Changes the current state and notifies all observers.
  void SetState(State state);

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

  // DBus callbacks.
  void OnArcAvailable(bool available);
  void OnInstanceStarted(bool success);
  void OnInstanceStopped(bool success);

  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<IPC::ChannelProxy> ipc_channel_;

  base::ObserverList<Observer> observer_list_;

  // If the user's session has started.
  bool session_started_;

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
