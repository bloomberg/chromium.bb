// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/common/bluetooth/bluetooth_error.h"
#include "content/public/browser/browser_message_filter.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace content {

// Dispatches and sends bluetooth related messages sent to/from a child
// process BluetoothDispatcher from/to the main browser process.
//
// Intended to be instantiated by the RenderProcessHost and installed as
// a filter on the channel. BrowserMessageFilter is refcounted and typically
// lives as long as it is installed on a channel.
//
// UI Thread Note:
// BluetoothDispatcherHost is constructed, operates, and destroyed on the UI
// thread because BluetoothAdapter and related objects live there.
class CONTENT_EXPORT BluetoothDispatcherHost final
    : public BrowserMessageFilter,
      public device::BluetoothAdapter::Observer {
 public:
  BluetoothDispatcherHost();
  // BrowserMessageFilter:
  void OnDestruct() const override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SetBluetoothAdapterForTesting(const std::string& name);

 protected:
  ~BluetoothDispatcherHost() override;

 private:
  friend class base::DeleteHelper<BluetoothDispatcherHost>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;

  // Set |adapter_| to a BluetoothAdapter instance and register observers,
  // releasing references to previous |adapter_|.
  void set_adapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDevice(int thread_id, int request_id);
  void OnConnectGATT(int thread_id, int request_id,
                     const std::string& device_instance_id);

  // Callbacks for BluetoothAdapter::StartDiscoverySession.
  void OnDiscoverySessionStarted(
      int thread_id,
      int request_id,
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnDiscoverySessionStartedError(int thread_id, int request_id);

  // Stop in progress discovery session.
  void StopDiscoverySession(
      int thread_id,
      int request_id,
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  // Callbacks for BluetoothDiscoverySession::Stop.
  void OnDiscoverySessionStopped(int thread_id, int request_id);
  void OnDiscoverySessionStoppedError(int thread_id, int request_id);

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  enum class MockData { NOT_MOCKING, REJECT, RESOLVE };
  MockData bluetooth_mock_data_set_;
  BluetoothError bluetooth_request_device_reject_type_;

  // Must be last member, see base/memory/weak_ptr.h documentation
  base::WeakPtrFactory<BluetoothDispatcherHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
