// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/browser_message_filter.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
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

  struct CacheQueryResult;
  struct RequestDeviceSession;
  struct PrimaryServicesRequest;

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
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  // Sends an IPC to the thread informing that a the characteristic's
  // value changed.
  void NotifyActiveCharacteristic(int thread_id,
                                  const std::string& characteristic_instance_id,
                                  const std::vector<uint8_t>& value);

  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDevice(
      int thread_id,
      int request_id,
      int frame_routing_id,
      const std::vector<content::BluetoothScanFilter>& filters,
      const std::vector<device::BluetoothUUID>& optional_services);
  void OnConnectGATT(int thread_id,
                     int request_id,
                     int frame_routing_id,
                     const std::string& device_id);
  void OnGetPrimaryService(int thread_id,
                           int request_id,
                           int frame_routing_id,
                           const std::string& device_id,
                           const std::string& service_uuid);
  void OnGetCharacteristic(int thread_id,
                           int request_id,
                           int frame_routing_id,
                           const std::string& service_instance_id,
                           const std::string& characteristic_uuid);
  void OnReadValue(int thread_id,
                   int request_id,
                   int frame_routing_id,
                   const std::string& characteristic_instance_id);
  void OnWriteValue(int thread_id,
                    int request_id,
                    int frame_routing_id,
                    const std::string& characteristic_instance_id,
                    const std::vector<uint8_t>& value);
  void OnStartNotifications(int thread_id,
                            int request_id,
                            int frame_routing_id,
                            const std::string& characteristic_instance_id);
  void OnStopNotifications(int thread_id,
                           int request_id,
                           int frame_routing_id,
                           const std::string& characteristic_instance_id);
  void OnRegisterCharacteristicObject(
      int thread_id,
      int frame_routing_id,
      const std::string& characteristic_instance_id);
  void OnUnregisterCharacteristicObject(
      int thread_id,
      int frame_routing_id,
      const std::string& characteristic_instance_id);

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
      const std::string& device_id,
      base::TimeTicks start_time,
      scoped_ptr<device::BluetoothGattConnection> connection);
  void OnCreateGATTConnectionError(
      int thread_id,
      int request_id,
      const std::string& device_id,
      base::TimeTicks start_time,
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Adds the service to the map of services' instance ids to devices' instance
  // ids and sends the service to the renderer.
  void AddToServicesMapAndSendGetPrimaryServiceSuccess(
      const device::BluetoothGattService& service,
      int thread_id,
      int request_id);

  // Callbacks for BluetoothGattCharacteristic::ReadRemoteCharacteristic.
  void OnCharacteristicValueRead(int thread_id,
                                 int request_id,
                                 const std::vector<uint8_t>& value);
  void OnCharacteristicReadValueError(
      int thread_id,
      int request_id,
      device::BluetoothGattService::GattErrorCode);

  // Callbacks for BluetoothGattCharacteristic::WriteRemoteCharacteristic.
  void OnWriteValueSuccess(int thread_id, int request_id);
  void OnWriteValueFailed(int thread_id,
                          int request_id,
                          device::BluetoothGattService::GattErrorCode);

  // Callbacks for BluetoothGattCharacteristic::StartNotifySession.
  void OnStartNotifySessionSuccess(
      int thread_id,
      int request_id,
      scoped_ptr<device::BluetoothGattNotifySession> notify_session);
  void OnStartNotifySessionFailed(
      int thread_id,
      int request_id,
      device::BluetoothGattService::GattErrorCode error_code);

  // Callback for BluetoothGattNotifySession::Stop.
  void OnStopNotifySession(int thread_id,
                           int request_id,
                           const std::string& characteristic_instance_id);

  // Functions to query the platform cache for the bluetooth object.
  // result.outcome == CacheQueryOutcome::SUCCESS if the object was found in the
  // cache. Otherwise result.outcome that can used to record the outcome and
  // result.error will contain the error that should be send to the renderer.
  // One of the possible outcomes is BAD_RENDERER. In this case the outcome
  // was already recorded and since there renderer crashed there is no need to
  // send a response.

  // Queries the platform cache for a Device with |device_id| for |origin|.
  // Fills in the |outcome| field and the |device| field if successful.
  CacheQueryResult QueryCacheForDevice(const url::Origin& origin,
                                       const std::string& device_id);
  // Queries the platform cache for a Service with |service_instance_id|. Fills
  // in the |outcome| field, and |device| and |service| fields if successful.
  CacheQueryResult QueryCacheForService(const url::Origin& origin,
                                        const std::string& service_instance_id);
  // Queries the platform cache for a characteristic with
  // |characteristic_instance_id|. Fills in the |outcome| field, and |device|,
  // |service| and |characteristic| fields if successful.
  CacheQueryResult QueryCacheForCharacteristic(
      const url::Origin& origin,
      const std::string& characteristic_instance_id);

  // Adds the PrimaryServicesRequest to the vector of pending services requests
  // for that device.
  void AddToPendingPrimaryServicesRequest(
      const std::string& device_address,
      const PrimaryServicesRequest& request);

  // Returns the origin for the frame with "frame_routing_id" in
  // render_process_id_.
  url::Origin GetOrigin(int frame_routing_id);

  // Returns true if the frame has permission to access the characteristic
  // with |characteristic_instance_id|.
  bool CanFrameAccessCharacteristicInstance(
      int frame_routing_id,
      const std::string& characteristic_instance_id);

  // Show help pages from the chooser dialog.
  void ShowBluetoothOverviewLink();
  void ShowBluetoothPairingLink();
  void ShowBluetoothAdapterOffLink();
  void ShowNeedLocationLink();

  int render_process_id_;

  // Maps a (thread_id,request_id) to information about its requestDevice call,
  // including the chooser dialog.
  // An entry is added to this map in OnRequestDevice, and should be removed
  // again everywhere a requestDevice() reply is sent.
  IDMap<RequestDeviceSession, IDMapOwnPointer> request_device_sessions_;

  BluetoothAllowedDevicesMap allowed_devices_map_;

  // Maps to get the object's parent based on it's instanceID
  // Map of service_instance_id to device_address.
  std::map<std::string, std::string> service_to_device_;
  // Map of characteristic_instance_id to service_instance_id.
  std::map<std::string, std::string> characteristic_to_service_;

  // Map that matches characteristic_instance_id to notify session.
  // TODO(ortuno): Also key by thread_id once support for web workers,
  // is added: http://crbug.com/537372
  std::map<std::string, scoped_ptr<device::BluetoothGattNotifySession>>
      characteristic_id_to_notify_session_;

  // Map of characteristic_instance_id to a set of thread ids.
  // A thread_id in the set represents a BluetoothDispatcher that
  // needs to be notified of changes to the characteristic.
  std::map<std::string, std::set<int>> active_characteristic_threads_;

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

  // Retain BluetoothGattConnection objects to keep connections open.
  // TODO(scheib): Destroy as connections are closed. http://crbug.com/539643
  ScopedVector<device::BluetoothGattConnection> connections_;

  // Map of device_address's to primary-services requests that need responses
  // when that device's service discovery completes.
  std::map<std::string, std::vector<PrimaryServicesRequest>>
      pending_primary_services_requests_;

  // |weak_ptr_on_ui_thread_| provides weak pointers, e.g. for callbacks, and
  // because it exists and has been bound to the UI thread enforces that all
  // copies verify they are also used on the UI thread.
  base::WeakPtr<BluetoothDispatcherHost> weak_ptr_on_ui_thread_;
  base::WeakPtrFactory<BluetoothDispatcherHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
