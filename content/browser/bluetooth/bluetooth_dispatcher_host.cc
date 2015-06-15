// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NETWORK_ERROR Note:
// When a device can't be found in the BluetoothAdapter, that generally
// indicates that it's gone out of range. We reject with a NetworkError in that
// case.
// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothdevice-connectgatt

#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/bad_message.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattService;

namespace {

// These types of errors aren't as common. We log them to understand
// how common they are and if we need to investigate more.
enum class BluetoothGATTError {
  UNKNOWN,
  FAILED,
  IN_PROGRESS,
  NOT_PAIRED,
  // Add errors above this line and update corresponding histograms.xml enum.
  MAX_ERROR,
};

// TODO(ortuno): Once we have a chooser for scanning and the right
// callback for discovered services we should delete these constants.
// https://crbug.com/436280 and https://crbug.com/484504
const int kDelayTime = 5;         // 5 seconds for scanning and discovering
const int kTestingDelayTime = 0;  // No need to wait during tests

// Error Messages
const char kNoBluetoothAdapter[] = "Bluetooth adapter not available.";

const char kDiscoverySessionStartFailed[] = "Couldn't start discovery session.";
const char kDiscoverySessionStopFailed[] = "Failed to stop discovery session.";

const char kNoDevicesFound[] = "No Bluetooth devices in range.";
const char kServiceNotFound[] = "Service not found in device";
const char kCharacteristicNotFound[] = "Characteristic not found in device.";

const char kDeviceNoLongerInRange[] = "Bluetooth Device is no longer in range.";
const char kServiceNoLongerExists[] = "GATT Service no longer exists.";
const char kCharacteristicNoLongerExits[] =
    "GATT Characteristic no longer exists";

void AddToHistogram(BluetoothGATTError error) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.GATTErrors", static_cast<int>(error),
                            static_cast<int>(BluetoothGATTError::MAX_ERROR));
}

std::string GetConnectErrorMessage(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ERROR_UNKNOWN:
      return "Unknown error when connecting to the device.";
    case device::BluetoothDevice::ERROR_INPROGRESS:
      return "Connection already in progress.";
    case device::BluetoothDevice::ERROR_FAILED:
      return "Connection failed for unknown reason.";
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      return "Authentication failed.";
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      return "Authentication canceled.";
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      return "Authentication rejected.";
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      return "Authentication timeout.";
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      return "Unsupported device.";
  }
  NOTREACHED();
  return "";
}

std::pair<content::BluetoothError, std::string> TranslateGATTError(
    BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case BluetoothGattService::GATT_ERROR_UNKNOWN:
      AddToHistogram(BluetoothGATTError::UNKNOWN);
      return std::make_pair(content::BluetoothError::NOT_SUPPORTED,
                            "GATT Error Unknown.");
    case BluetoothGattService::GATT_ERROR_FAILED:
      AddToHistogram(BluetoothGATTError::FAILED);
      return std::make_pair(content::BluetoothError::NOT_SUPPORTED,
                            "GATT operation failed for unknown reason.");
    case BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      AddToHistogram(BluetoothGATTError::IN_PROGRESS);
      return std::make_pair(content::BluetoothError::NETWORK,
                            "GATT operation already in progress.");
    case BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      return std::make_pair(content::BluetoothError::INVALID_MODIFICATION,
                            "GATT Error: invalid attribute length.");
    case BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      return std::make_pair(content::BluetoothError::NOT_SUPPORTED,
                            "GATT operation not permitted.");
    case BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      return std::make_pair(content::BluetoothError::SECURITY,
                            "GATT operation not authorized");
    case BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      AddToHistogram(BluetoothGATTError::NOT_PAIRED);
      return std::make_pair(content::BluetoothError::NETWORK,
                            "GATT Error: Not paired.");
    case BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      return std::make_pair(content::BluetoothError::NOT_SUPPORTED,
                            "GATT Error: Not supported.");
  }
  NOTREACHED();
  return std::make_pair(content::BluetoothError::NOT_SUPPORTED, "");
}

}  //  namespace

namespace content {

BluetoothDispatcherHost::BluetoothDispatcherHost()
    : BrowserMessageFilter(BluetoothMsgStart),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  current_delay_time_ = kDelayTime;
  if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothDispatcherHost::set_adapter,
                   weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDispatcherHost::OnDestruct() const {
  // See class comment: UI Thread Note.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void BluetoothDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  // See class comment: UI Thread Note.
  *thread = BrowserThread::UI;
}

bool BluetoothDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BluetoothDispatcherHost, message)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_RequestDevice, OnRequestDevice)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_ConnectGATT, OnConnectGATT)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GetPrimaryService, OnGetPrimaryService)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GetCharacteristic, OnGetCharacteristic)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_ReadValue, OnReadValue)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BluetoothDispatcherHost::SetBluetoothAdapterForTesting(
    scoped_refptr<device::BluetoothAdapter> mock_adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  current_delay_time_ = kTestingDelayTime;
  set_adapter(mock_adapter.Pass());
}

BluetoothDispatcherHost::~BluetoothDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Clear adapter, releasing observer references.
  set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

void BluetoothDispatcherHost::set_adapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (adapter_.get())
    adapter_->RemoveObserver(this);
  adapter_ = adapter;
  if (adapter_.get())
    adapter_->AddObserver(this);
}

void BluetoothDispatcherHost::OnRequestDevice(int thread_id, int request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(scheib): Filter devices by services: crbug.com/440594
  // TODO(scheib): Device selection UI: crbug.com/436280
  // TODO(scheib): Utilize BluetoothAdapter::Observer::DeviceAdded/Removed.
  if (adapter_.get()) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStarted,
                   weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
        base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStartedError,
                   weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
  } else {
    DLOG(WARNING) << "No BluetoothAdapter. Can't serve requestDevice.";
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id, BluetoothError::NOT_FOUND, kNoBluetoothAdapter));
  }
  return;
}

void BluetoothDispatcherHost::OnConnectGATT(
    int thread_id,
    int request_id,
    const std::string& device_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(ortuno): Right now it's pointless to check if the domain has access to
  // the device, because any domain can connect to any device. But once
  // permissions are implemented we should check that the domain has access to
  // the device. https://crbug.com/484745
  device::BluetoothDevice* device = adapter_->GetDevice(device_instance_id);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    Send(new BluetoothMsg_ConnectGATTError(thread_id, request_id,
                                           BluetoothError::NETWORK,
                                           kDeviceNoLongerInRange));
    return;
  }
  device->CreateGattConnection(
      base::Bind(&BluetoothDispatcherHost::OnGATTConnectionCreated,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 device_instance_id),
      base::Bind(&BluetoothDispatcherHost::OnCreateGATTConnectionError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 device_instance_id));
}

void BluetoothDispatcherHost::OnGetPrimaryService(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    const std::string& service_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(ortuno): Check if device_instance_id is in "allowed devices"
  // https://crbug.com/493459
  // TODO(ortuno): Check if service_uuid is in "allowed services"
  // https://crbug.com/493460
  // For now just wait a fixed time and call OnServiceDiscovered.
  // TODO(ortuno): Use callback once it's implemented http://crbug.com/484504
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BluetoothDispatcherHost::OnServicesDiscovered,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 device_instance_id, service_uuid),
      base::TimeDelta::FromSeconds(current_delay_time_));
}

void BluetoothDispatcherHost::OnGetCharacteristic(
    int thread_id,
    int request_id,
    const std::string& service_instance_id,
    const std::string& characteristic_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto device_iter = service_to_device_.find(service_instance_id);
  // A service_instance_id not in the map implies a hostile renderer
  // because a renderer obtains the service id from this class and
  // it will be added to the map at that time.
  if (device_iter == service_to_device_.end()) {
    // Kill the renderer
    bad_message::ReceivedBadMessage(this, bad_message::BDH_INVALID_SERVICE_ID);
    return;
  }

  // TODO(ortuno): Check if domain has access to device.
  // https://crbug.com/493459
  device::BluetoothDevice* device =
      adapter_->GetDevice(device_iter->second /* device_instance_id */);

  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    Send(new BluetoothMsg_GetCharacteristicError(thread_id, request_id,
                                                 BluetoothError::NETWORK,
                                                 kDeviceNoLongerInRange));
    return;
  }

  // TODO(ortuno): Check if domain has access to service
  // http://crbug.com/493460
  device::BluetoothGattService* service =
      device->GetGattService(service_instance_id);
  if (!service) {
    Send(new BluetoothMsg_GetCharacteristicError(thread_id, request_id,
                                                 BluetoothError::INVALID_STATE,
                                                 kServiceNoLongerExists));
    return;
  }

  for (BluetoothGattCharacteristic* characteristic :
       service->GetCharacteristics()) {
    if (characteristic->GetUUID().canonical_value() == characteristic_uuid) {
      const std::string& characteristic_instance_id =
          characteristic->GetIdentifier();

      auto insert_result = characteristic_to_service_.insert(
          make_pair(characteristic_instance_id, service_instance_id));

      // If  value is already in map, DCHECK it's valid.
      if (!insert_result.second)
        DCHECK(insert_result.first->second == service_instance_id);

      // TODO(ortuno): Use generated instance ID instead.
      // https://crbug.com/495379
      Send(new BluetoothMsg_GetCharacteristicSuccess(
          thread_id, request_id, characteristic_instance_id));
      return;
    }
  }
  Send(new BluetoothMsg_GetCharacteristicError(thread_id, request_id,
                                               BluetoothError::NOT_FOUND,
                                               kCharacteristicNotFound));
}

void BluetoothDispatcherHost::OnReadValue(
    int thread_id,
    int request_id,
    const std::string& characteristic_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto characteristic_iter =
      characteristic_to_service_.find(characteristic_instance_id);
  // A characteristic_instance_id not in the map implies a hostile renderer
  // because a renderer obtains the characteristic id from this class and
  // it will be added to the map at that time.
  if (characteristic_iter == characteristic_to_service_.end()) {
    // Kill the renderer
    bad_message::ReceivedBadMessage(this,
                                    bad_message::BDH_INVALID_CHARACTERISTIC_ID);
    return;
  }
  const std::string& service_instance_id = characteristic_iter->second;

  auto device_iter = service_to_device_.find(service_instance_id);

  CHECK(device_iter != service_to_device_.end());

  device::BluetoothDevice* device =
      adapter_->GetDevice(device_iter->second /* device_instance_id */);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    Send(new BluetoothMsg_ReadCharacteristicValueError(thread_id, request_id,
                                                       BluetoothError::NETWORK,
                                                       kDeviceNoLongerInRange));
    return;
  }

  BluetoothGattService* service = device->GetGattService(service_instance_id);
  if (service == nullptr) {
    Send(new BluetoothMsg_ReadCharacteristicValueError(
        thread_id, request_id, BluetoothError::INVALID_STATE,
        kServiceNoLongerExists));
    return;
  }

  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_instance_id);
  if (characteristic == nullptr) {
    Send(new BluetoothMsg_ReadCharacteristicValueError(
        thread_id, request_id, BluetoothError::INVALID_STATE,
        kCharacteristicNoLongerExits));
    return;
  }

  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothDispatcherHost::OnCharacteristicValueRead,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
      base::Bind(&BluetoothDispatcherHost::OnCharacteristicReadValueError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
}

void BluetoothDispatcherHost::OnDiscoverySessionStarted(
    int thread_id,
    int request_id,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BluetoothDispatcherHost::StopDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 base::Passed(&discovery_session)),
      base::TimeDelta::FromSeconds(current_delay_time_));
}

void BluetoothDispatcherHost::OnDiscoverySessionStartedError(int thread_id,
                                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(WARNING) << "BluetoothDispatcherHost::OnDiscoverySessionStartedError";
  Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                           BluetoothError::NOT_FOUND,
                                           kDiscoverySessionStartFailed));
}

void BluetoothDispatcherHost::StopDiscoverySession(
    int thread_id,
    int request_id,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  discovery_session->Stop(
      base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStopped,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
      base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStoppedError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
}

void BluetoothDispatcherHost::OnDiscoverySessionStopped(int thread_id,
                                                        int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  if (devices.begin() == devices.end()) {
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id, BluetoothError::NOT_FOUND, kNoDevicesFound));
  } else {
    device::BluetoothDevice* device = *devices.begin();
    content::BluetoothDevice device_ipc(
        device->GetAddress(),         // instance_id
        device->GetName(),            // name
        device->GetBluetoothClass(),  // device_class
        device->GetVendorIDSource(),  // vendor_id_source
        device->GetVendorID(),        // vendor_id
        device->GetProductID(),       // product_id
        device->GetDeviceID(),        // product_version
        device->IsPaired(),           // paired
        content::BluetoothDevice::UUIDsFromBluetoothUUIDs(
            device->GetUUIDs()));  // uuids
    Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                               device_ipc));
  }
}

void BluetoothDispatcherHost::OnDiscoverySessionStoppedError(int thread_id,
                                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(WARNING) << "BluetoothDispatcherHost::OnDiscoverySessionStoppedError";
  Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                           BluetoothError::NOT_FOUND,
                                           kDiscoverySessionStopFailed));
}

void BluetoothDispatcherHost::OnGATTConnectionCreated(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    scoped_ptr<device::BluetoothGattConnection> connection) {
  // TODO(ortuno): Save the BluetoothGattConnection so we can disconnect
  // from it.
  Send(new BluetoothMsg_ConnectGATTSuccess(thread_id, request_id,
                                           device_instance_id));
}

void BluetoothDispatcherHost::OnCreateGATTConnectionError(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  // There was an error creating the ATT Bearer so we reject with
  // NetworkError.
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothdevice-connectgatt
  Send(new BluetoothMsg_ConnectGATTError(thread_id, request_id,
                                         BluetoothError::NETWORK,
                                         GetConnectErrorMessage(error_code)));
}

void BluetoothDispatcherHost::OnServicesDiscovered(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    const std::string& service_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device::BluetoothDevice* device = adapter_->GetDevice(device_instance_id);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    Send(new BluetoothMsg_GetPrimaryServiceError(thread_id, request_id,
                                                 BluetoothError::NETWORK,
                                                 kDeviceNoLongerInRange));
    return;
  }
  for (BluetoothGattService* service : device->GetGattServices()) {
    if (service->GetUUID().canonical_value() == service_uuid) {
      // TODO(ortuno): Use generated instance ID instead.
      // https://crbug.com/495379
      const std::string& service_identifier = service->GetIdentifier();
      auto insert_result = service_to_device_.insert(
          make_pair(service_identifier, device_instance_id));

      // If a value is already in map, DCHECK it's valid.
      if (!insert_result.second)
        DCHECK(insert_result.first->second == device_instance_id);

      Send(new BluetoothMsg_GetPrimaryServiceSuccess(thread_id, request_id,
                                                     service_identifier));
      return;
    }
  }
  Send(new BluetoothMsg_GetPrimaryServiceError(
      thread_id, request_id, BluetoothError::NOT_FOUND, kServiceNotFound));
}

void BluetoothDispatcherHost::OnCharacteristicValueRead(
    int thread_id,
    int request_id,
    const std::vector<uint8>& value) {
  Send(new BluetoothMsg_ReadCharacteristicValueSuccess(thread_id, request_id,
                                                       value));
}

void BluetoothDispatcherHost::OnCharacteristicReadValueError(
    int thread_id,
    int request_id,
    device::BluetoothGattService::GattErrorCode error_code) {
  std::pair<BluetoothError, std::string> error = TranslateGATTError(error_code);
  Send(new BluetoothMsg_ReadCharacteristicValueError(
      thread_id, request_id, error.first, error.second));
}

}  // namespace content
