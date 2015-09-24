// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/browser_message_filter.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace device {
class BluetoothUUID;
}

namespace content {

struct BluetoothScanFilter;

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
  BluetoothDispatcherHost(int render_process_id);
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

  struct RequestDeviceSession;

  // Set |adapter_| to a BluetoothAdapter instance and register observers,
  // releasing references to previous |adapter_|.
  void set_adapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Makes sure a BluetoothDiscoverySession is active for |session|, and resets
  // its timeout.
  void StartDeviceDiscovery(RequestDeviceSession* session, int chooser_id);

  // Stops all BluetoothDiscoverySessions being run for requestDevice()
  // choosers.
  void StopDeviceDiscovery();

  // BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDevice(
      int thread_id,
      int request_id,
      int frame_routing_id,
      const std::vector<content::BluetoothScanFilter>& filters,
      const std::vector<device::BluetoothUUID>& optional_services);
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
  void OnReadValue(int thread_id,
                   int request_id,
                   const std::string& characteristic_instance_id);
  void OnWriteValue(int thread_id,
                    int request_id,
                    const std::string& characteristic_instance_id,
                    const std::vector<uint8_t>& value);

  // Callbacks for BluetoothAdapter::StartDiscoverySession.
  void OnDiscoverySessionStarted(
      int chooser_id,
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnDiscoverySessionStartedError(int chooser_id);

  // BluetoothChooser::EventHandler:
  void OnBluetoothChooserEvent(int chooser_id,
                               BluetoothChooser::Event event,
                               const std::string& device_id);

  // The chooser implementation yields to the event loop to avoid re-entering
  // code that's still using the RequestDeviceSession, and continues with this
  // function.
  void FinishClosingChooser(int chooser_id,
                            BluetoothChooser::Event event,
                            const std::string& device_id);

  // Callbacks for BluetoothDevice::CreateGattConnection.
  void OnGATTConnectionCreated(
      int thread_id,
      int request_id,
      const std::string& device_instance_id,
      base::TimeTicks start_time,
      scoped_ptr<device::BluetoothGattConnection> connection);
  void OnCreateGATTConnectionError(
      int thread_id,
      int request_id,
      const std::string& device_instance_id,
      base::TimeTicks start_time,
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callback for future BluetoothAdapter::ServicesDiscovered callback:
  // For now we just post a delayed task.
  // See: https://crbug.com/484504
  void OnServicesDiscovered(int thread_id,
                            int request_id,
                            const std::string& device_instance_id,
                            const std::string& service_uuid);

  // Callbacks for BluetoothGattCharacteristic::ReadRemoteCharacteristic.
  void OnCharacteristicValueRead(int thread_id,
                                 int request_id,
                                 const std::vector<uint8>& value);
  void OnCharacteristicReadValueError(
      int thread_id,
      int request_id,
      device::BluetoothGattService::GattErrorCode);

  // Callbacks for BluetoothGattCharacteristic::WriteRemoteCharacteristic.
  void OnWriteValueSuccess(int thread_id, int request_id);
  void OnWriteValueFailed(int thread_id,
                          int request_id,
                          device::BluetoothGattService::GattErrorCode);

  // Show help pages from the chooser dialog.
  void ShowBluetoothOverviewLink();
  void ShowBluetoothPairingLink();
  void ShowBluetoothAdapterOffLink();

  int render_process_id_;

  // Maps a (thread_id,request_id) to information about its requestDevice call,
  // including the chooser dialog.
  // An entry is added to this map in OnRequestDevice, and should be removed
  // again everywhere a requestDevice() reply is sent.
  IDMap<RequestDeviceSession, IDMapOwnPointer> request_device_sessions_;

  // Maps to get the object's parent based on it's instanceID
  // Map of service_instance_id to device_instance_id.
  std::map<std::string, std::string> service_to_device_;
  // Map of characteristic_instance_id to service_instance_id.
  std::map<std::string, std::string> characteristic_to_service_;

  // Defines how long to scan for and how long to discover services for.
  int current_delay_time_;

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Automatically stops Bluetooth discovery a set amount of time after it was
  // started. We have a single timer for all of Web Bluetooth because it's
  // simpler than tracking timeouts for each RequestDeviceSession individually,
  // and because there's no harm in extending the length of a few discovery
  // sessions when other sessions are active.
  base::Timer discovery_session_timer_;

  // Must be last member, see base/memory/weak_ptr.h documentation
  base::WeakPtrFactory<BluetoothDispatcherHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
