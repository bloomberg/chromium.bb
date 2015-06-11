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
#include "device/bluetooth/bluetooth_gatt_connection.h"

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

  void SetBluetoothAdapterForTesting(
      scoped_refptr<device::BluetoothAdapter> mock_adapter);

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
  void OnGetPrimaryService(int thread_id,
                           int request_id,
                           const std::string& device_instance_id,
                           const std::string& service_uuid);
  void OnGetCharacteristic(int thread_id,
                           int request_id,
                           const std::string& service_instance_id,
                           const std::string& characteristic_uuid);

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

  // Callbacks for BluetoothDevice::CreateGattConnection
  void OnGATTConnectionCreated(
      int thread_id,
      int request_id,
      const std::string& device_instance_id,
      scoped_ptr<device::BluetoothGattConnection> connection);
  void OnCreateGATTConnectionError(
      int thread_id,
      int request_id,
      const std::string& device_instance_id,
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callback for future BluetoothAdapter::ServicesDiscovered callback:
  // For now we just post a delayed task.
  // See: https://crbug.com/484504
  void OnServicesDiscovered(int thread_id,
                            int request_id,
                            const std::string& device_instance_id,
                            const std::string& service_uuid);

  // Maps to get the object's parent based on it's instanceID
  // Map of service_instance_id to device_instance_id.
  std::map<std::string, std::string> service_to_device_;

  // Defines how long to scan for and how long to discover services for.
  int current_delay_time_;

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Must be last member, see base/memory/weak_ptr.h documentation
  base::WeakPtrFactory<BluetoothDispatcherHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
