// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ID Not In Map Note:
// A service, characteristic, or descriptor ID not in the corresponding
// BluetoothDispatcherHost map [service_to_device_, characteristic_to_service_,
// descriptor_to_characteristic_] implies a hostile renderer because a renderer
// obtains the corresponding ID from this class and it will be added to the map
// at that time.

#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/bad_message.h"
#include "content/browser/bluetooth/bluetooth_blacklist.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/cache_query_result.h"
#include "content/browser/bluetooth/first_device_bluetooth_chooser.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

using blink::WebBluetoothError;
using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattService;
using device::BluetoothUUID;

namespace content {

namespace {

// TODO(ortuno): Once we have a chooser for scanning, a way to control that
// chooser from tests, and the right callback for discovered services we should
// delete these constants.
// https://crbug.com/436280 and https://crbug.com/484504
const int kDelayTime = 5;         // 5 seconds for scanning and discovering
const int kTestingDelayTime = 0;  // No need to wait during tests
const size_t kMaxLengthForDeviceName =
    29;  // max length of device name in filter.

bool IsEmptyOrInvalidFilter(const content::BluetoothScanFilter& filter) {
  return filter.name.empty() && filter.namePrefix.empty() &&
         filter.services.empty() &&
         filter.name.length() > kMaxLengthForDeviceName &&
         filter.namePrefix.length() > kMaxLengthForDeviceName;
}

bool HasEmptyOrInvalidFilter(
    const std::vector<content::BluetoothScanFilter>& filters) {
  return filters.empty()
             ? true
             : filters.end() != std::find_if(filters.begin(), filters.end(),
                                             IsEmptyOrInvalidFilter);
}

// Defined at
// https://webbluetoothchrome.github.io/web-bluetooth/#dfn-matches-a-filter
bool MatchesFilter(const device::BluetoothDevice& device,
                   const content::BluetoothScanFilter& filter) {
  DCHECK(!IsEmptyOrInvalidFilter(filter));

  const std::string device_name = base::UTF16ToUTF8(device.GetName());

  if (!filter.name.empty() && (device_name != filter.name)) {
      return false;
  }

  if (!filter.namePrefix.empty() &&
      (!base::StartsWith(device_name, filter.namePrefix,
                         base::CompareCase::SENSITIVE))) {
    return false;
  }

  if (!filter.services.empty()) {
    const auto& device_uuid_list = device.GetUUIDs();
    const std::set<BluetoothUUID> device_uuids(device_uuid_list.begin(),
                                               device_uuid_list.end());
    for (const auto& service : filter.services) {
      if (!ContainsKey(device_uuids, service)) {
        return false;
      }
    }
  }

  return true;
}

bool MatchesFilters(const device::BluetoothDevice& device,
                    const std::vector<content::BluetoothScanFilter>& filters) {
  DCHECK(!HasEmptyOrInvalidFilter(filters));
  for (const content::BluetoothScanFilter& filter : filters) {
    if (MatchesFilter(device, filter)) {
      return true;
    }
  }
  return false;
}

WebBluetoothError TranslateConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ERROR_UNKNOWN:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNKNOWN);
      return WebBluetoothError::CONNECT_UNKNOWN_ERROR;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::IN_PROGRESS);
      return WebBluetoothError::CONNECT_ALREADY_IN_PROGRESS;
    case device::BluetoothDevice::ERROR_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::FAILED);
      return WebBluetoothError::CONNECT_UNKNOWN_FAILURE;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_FAILED);
      return WebBluetoothError::CONNECT_AUTH_FAILED;
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_CANCELED);
      return WebBluetoothError::CONNECT_AUTH_CANCELED;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_REJECTED);
      return WebBluetoothError::CONNECT_AUTH_REJECTED;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_TIMEOUT);
      return WebBluetoothError::CONNECT_AUTH_TIMEOUT;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNSUPPORTED_DEVICE);
      return WebBluetoothError::CONNECT_UNSUPPORTED_DEVICE;
    case device::BluetoothDevice::ERROR_ATTRIBUTE_LENGTH_INVALID:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::ATTRIBUTE_LENGTH_INVALID);
      return WebBluetoothError::CONNECT_ATTRIBUTE_LENGTH_INVALID;
    case device::BluetoothDevice::ERROR_CONNECTION_CONGESTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::CONNECTION_CONGESTED);
      return WebBluetoothError::CONNECT_CONNECTION_CONGESTED;
    case device::BluetoothDevice::ERROR_INSUFFICIENT_ENCRYPTION:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::INSUFFICIENT_ENCRYPTION);
      return WebBluetoothError::CONNECT_INSUFFICIENT_ENCRYPTION;
    case device::BluetoothDevice::ERROR_OFFSET_INVALID:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::OFFSET_INVALID);
      return WebBluetoothError::CONNECT_OFFSET_INVALID;
    case device::BluetoothDevice::ERROR_READ_NOT_PERMITTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::READ_NOT_PERMITTED);
      return WebBluetoothError::CONNECT_READ_NOT_PERMITTED;
    case device::BluetoothDevice::ERROR_REQUEST_NOT_SUPPORTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::REQUEST_NOT_SUPPORTED);
      return WebBluetoothError::CONNECT_REQUEST_NOT_SUPPORTED;
    case device::BluetoothDevice::ERROR_WRITE_NOT_PERMITTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::WRITE_NOT_PERMITTED);
      return WebBluetoothError::CONNECT_WRITE_NOT_PERMITTED;
    case device::BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      return WebBluetoothError::UNTRANSLATED_CONNECT_ERROR_CODE;
  }
  NOTREACHED();
  return WebBluetoothError::UNTRANSLATED_CONNECT_ERROR_CODE;
}

void StopDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // Nothing goes wrong if the discovery session fails to stop, and we don't
  // need to wait for it before letting the user's script proceed, so we ignore
  // the results here.
  discovery_session->Stop(base::Bind(&base::DoNothing),
                          base::Bind(&base::DoNothing));
}

// TODO(ortuno): This should really be a BluetoothDevice method.
// Replace when implemented. http://crbug.com/552022
std::vector<BluetoothRemoteGattService*> GetPrimaryServicesByUUID(
    device::BluetoothDevice* device,
    const std::string& service_uuid) {
  std::vector<BluetoothRemoteGattService*> services;
  VLOG(1) << "Looking for service: " << service_uuid;
  for (BluetoothRemoteGattService* service : device->GetGattServices()) {
    VLOG(1) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->GetUUID().canonical_value() == service_uuid &&
        service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

UMARequestDeviceOutcome OutcomeFromChooserEvent(BluetoothChooser::Event event) {
  switch (event) {
    case BluetoothChooser::Event::DENIED_PERMISSION:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_DENIED_PERMISSION;
    case BluetoothChooser::Event::CANCELLED:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_CANCELLED;
    case BluetoothChooser::Event::SHOW_OVERVIEW_HELP:
      return UMARequestDeviceOutcome::BLUETOOTH_OVERVIEW_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SHOW_ADAPTER_OFF_HELP:
      return UMARequestDeviceOutcome::ADAPTER_OFF_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SHOW_NEED_LOCATION_HELP:
      return UMARequestDeviceOutcome::NEED_LOCATION_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SELECTED:
      // We can't know if we are going to send a success message yet because
      // the device could have vanished. This event should be histogramed
      // manually after checking if the device is still around.
      NOTREACHED();
      return UMARequestDeviceOutcome::SUCCESS;
    case BluetoothChooser::Event::RESCAN:
      // Rescanning doesn't result in a IPC message for the request being sent
      // so no need to histogram it.
      NOTREACHED();
      return UMARequestDeviceOutcome::SUCCESS;
  }
  NOTREACHED();
  return UMARequestDeviceOutcome::SUCCESS;
}

}  //  namespace

BluetoothDispatcherHost::BluetoothDispatcherHost(int render_process_id)
    : BrowserMessageFilter(BluetoothMsgStart),
      render_process_id_(render_process_id),
      current_delay_time_(kDelayTime),
      discovery_session_timer_(
          FROM_HERE,
          // TODO(jyasskin): Add a way for tests to control the dialog
          // directly, and change this to a reasonable discovery timeout.
          base::TimeDelta::FromSecondsD(current_delay_time_),
          base::Bind(&BluetoothDispatcherHost::StopDeviceDiscovery,
                     // base::Timer guarantees it won't call back after its
                     // destructor starts.
                     base::Unretained(this)),
          /*is_repeating=*/false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  connected_devices_map_.reset(new ConnectedDevicesMap(render_process_id));

  // Bind all future weak pointers to the UI thread.
  weak_ptr_on_ui_thread_ = weak_ptr_factory_.GetWeakPtr();
  weak_ptr_on_ui_thread_.get();  // Associates with UI thread.
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
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GATTServerConnect, OnGATTServerConnect)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GATTServerDisconnect,
                      OnGATTServerDisconnect)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GetPrimaryService, OnGetPrimaryService)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BluetoothDispatcherHost::SetBluetoothAdapterForTesting(
    scoped_refptr<device::BluetoothAdapter> mock_adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (mock_adapter.get()) {
    current_delay_time_ = kTestingDelayTime;
    // Reset the discovery session timer to use the new delay time.
    discovery_session_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSecondsD(current_delay_time_),
        base::Bind(&BluetoothDispatcherHost::StopDeviceDiscovery,
                   // base::Timer guarantees it won't call back after its
                   // destructor starts.
                   base::Unretained(this)));
  } else {
    // The following data structures are used to store pending operations.
    // They should never contain elements at the end of a test.
    DCHECK(request_device_sessions_.IsEmpty());
    DCHECK(pending_primary_services_requests_.empty());

    // The following data structures are cleaned up when a
    // device/service/characteristic is removed.
    // Since this can happen after the test is done and the cleanup function is
    // called, we clean them here.
    service_to_device_.clear();
    connected_devices_map_.reset(new ConnectedDevicesMap(render_process_id_));
    allowed_devices_map_ = BluetoothAllowedDevicesMap();
  }

  set_adapter(std::move(mock_adapter));
}

BluetoothDispatcherHost::~BluetoothDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Clear adapter, releasing observer references.
  set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

// Stores information associated with an in-progress requestDevice call. This
// will include the state of the active chooser dialog in a future patch.
struct BluetoothDispatcherHost::RequestDeviceSession {
 public:
  RequestDeviceSession(int thread_id,
                       int request_id,
                       url::Origin origin,
                       const std::vector<BluetoothScanFilter>& filters,
                       const std::vector<BluetoothUUID>& optional_services)
      : thread_id(thread_id),
        request_id(request_id),
        origin(origin),
        filters(filters),
        optional_services(optional_services) {}

  void AddFilteredDevice(const device::BluetoothDevice& device) {
    if (chooser && MatchesFilters(device, filters)) {
      chooser->AddDevice(device.GetAddress(), device.GetName());
    }
  }

  std::unique_ptr<device::BluetoothDiscoveryFilter> ComputeScanFilter() const {
    std::set<BluetoothUUID> services;
    for (const BluetoothScanFilter& filter : filters) {
      services.insert(filter.services.begin(), filter.services.end());
    }
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter(
        new device::BluetoothDiscoveryFilter(
            device::BluetoothDiscoveryFilter::TRANSPORT_DUAL));
    for (const BluetoothUUID& service : services) {
      discovery_filter->AddUUID(service);
    }
    return discovery_filter;
  }

  const int thread_id;
  const int request_id;
  const url::Origin origin;
  const std::vector<BluetoothScanFilter> filters;
  const std::vector<BluetoothUUID> optional_services;
  std::unique_ptr<BluetoothChooser> chooser;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session;
};

struct BluetoothDispatcherHost::PrimaryServicesRequest {
  enum CallingFunction { GET_PRIMARY_SERVICE, GET_PRIMARY_SERVICES };

  PrimaryServicesRequest(int thread_id,
                         int request_id,
                         const std::string& service_uuid,
                         PrimaryServicesRequest::CallingFunction func)
      : thread_id(thread_id),
        request_id(request_id),
        service_uuid(service_uuid),
        func(func) {}
  ~PrimaryServicesRequest() {}

  int thread_id;
  int request_id;
  std::string service_uuid;
  CallingFunction func;
};

BluetoothDispatcherHost::ConnectedDevicesMap::ConnectedDevicesMap(
    int render_process_id)
    : render_process_id_(render_process_id) {}

BluetoothDispatcherHost::ConnectedDevicesMap::~ConnectedDevicesMap() {
  for (auto frame_id_device_id : frame_ids_device_ids_) {
    DecrementBluetoothConnectedDeviceCount(frame_id_device_id.first);
  }
}

bool BluetoothDispatcherHost::ConnectedDevicesMap::HasActiveConnection(
    const std::string& device_id) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter != device_id_to_connection_map_.end()) {
    return connection_iter->second->IsConnected();
  }
  return false;
}

void BluetoothDispatcherHost::ConnectedDevicesMap::InsertOrReplace(
    int frame_routing_id,
    const std::string& device_id,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter == device_id_to_connection_map_.end()) {
    IncrementBluetoothConnectedDeviceCount(frame_routing_id);
    frame_ids_device_ids_.insert(std::make_pair(frame_routing_id, device_id));
  } else {
    device_id_to_connection_map_.erase(connection_iter);
  }
  device_id_to_connection_map_[device_id] = std::move(connection);
}

void BluetoothDispatcherHost::ConnectedDevicesMap::Remove(
    int frame_routing_id,
    const std::string& device_id) {
  if (device_id_to_connection_map_.erase(device_id)) {
    VLOG(1) << "Disconnecting device: " << device_id;
    DecrementBluetoothConnectedDeviceCount(frame_routing_id);
    frame_ids_device_ids_.erase(std::make_pair(frame_routing_id, device_id));
  }
}

void BluetoothDispatcherHost::ConnectedDevicesMap::
    IncrementBluetoothConnectedDeviceCount(int frame_routing_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, frame_routing_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  if (web_contents) {
    web_contents->IncrementBluetoothConnectedDeviceCount();
  }
}

void BluetoothDispatcherHost::ConnectedDevicesMap::
    DecrementBluetoothConnectedDeviceCount(int frame_routing_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, frame_routing_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  if (web_contents) {
    web_contents->DecrementBluetoothConnectedDeviceCount();
  }
}

void BluetoothDispatcherHost::set_adapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    for (device::BluetoothAdapter::Observer* observer : adapter_observers_) {
      adapter_->RemoveObserver(observer);
    }
  }
  adapter_ = adapter;
  if (adapter_.get()) {
    adapter_->AddObserver(this);
    for (device::BluetoothAdapter::Observer* observer : adapter_observers_) {
      adapter_->AddObserver(observer);
    }
  } else {
    // Notify that the adapter has been removed and observers should clean up
    // their state.
    for (device::BluetoothAdapter::Observer* observer : adapter_observers_) {
      observer->AdapterPresentChanged(nullptr, false);
    }
  }
}

void BluetoothDispatcherHost::StartDeviceDiscovery(
    RequestDeviceSession* session,
    int chooser_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (session->discovery_session) {
    // Already running; just increase the timeout.
    discovery_session_timer_.Reset();
  } else {
    session->chooser->ShowDiscoveryState(
        BluetoothChooser::DiscoveryState::DISCOVERING);
    adapter_->StartDiscoverySessionWithFilter(
        session->ComputeScanFilter(),
        base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStarted,
                   weak_ptr_on_ui_thread_, chooser_id),
        base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStartedError,
                   weak_ptr_on_ui_thread_, chooser_id));
  }
}

void BluetoothDispatcherHost::StopDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (IDMap<RequestDeviceSession, IDMapOwnPointer>::iterator iter(
           &request_device_sessions_);
       !iter.IsAtEnd(); iter.Advance()) {
    RequestDeviceSession* session = iter.GetCurrentValue();
    if (session->discovery_session) {
      StopDiscoverySession(std::move(session->discovery_session));
    }
    if (session->chooser) {
      session->chooser->ShowDiscoveryState(
          BluetoothChooser::DiscoveryState::IDLE);
    }
  }
}

void BluetoothDispatcherHost::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BluetoothChooser::AdapterPresence presence =
      powered ? BluetoothChooser::AdapterPresence::POWERED_ON
              : BluetoothChooser::AdapterPresence::POWERED_OFF;
  for (IDMap<RequestDeviceSession, IDMapOwnPointer>::iterator iter(
           &request_device_sessions_);
       !iter.IsAtEnd(); iter.Advance()) {
    RequestDeviceSession* session = iter.GetCurrentValue();

    // Stop ongoing discovery session if power is off.
    if (!powered && session->discovery_session) {
      StopDiscoverySession(std::move(session->discovery_session));
    }

    if (session->chooser)
      session->chooser->SetAdapterPresence(presence);
  }

  // Stop the timer so that we don't change the state of the chooser
  // when timer expires.
  if (!powered) {
    discovery_session_timer_.Stop();
  }
}

void BluetoothDispatcherHost::DeviceAdded(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "Adding device to all choosers: " << device->GetAddress();
  for (IDMap<RequestDeviceSession, IDMapOwnPointer>::iterator iter(
           &request_device_sessions_);
       !iter.IsAtEnd(); iter.Advance()) {
    RequestDeviceSession* session = iter.GetCurrentValue();
    session->AddFilteredDevice(*device);
  }
}

void BluetoothDispatcherHost::DeviceRemoved(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "Marking device removed on all choosers: " << device->GetAddress();
  for (IDMap<RequestDeviceSession, IDMapOwnPointer>::iterator iter(
           &request_device_sessions_);
       !iter.IsAtEnd(); iter.Advance()) {
    RequestDeviceSession* session = iter.GetCurrentValue();
    if (session->chooser) {
      session->chooser->RemoveDevice(device->GetAddress());
    }
  }
}

void BluetoothDispatcherHost::GattServicesDiscovered(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string& device_address = device->GetAddress();
  VLOG(1) << "Services discovered for device: " << device_address;

  auto iter = pending_primary_services_requests_.find(device_address);
  if (iter == pending_primary_services_requests_.end()) {
    return;
  }
  std::vector<PrimaryServicesRequest> requests;
  requests.swap(iter->second);
  pending_primary_services_requests_.erase(iter);

  for (const PrimaryServicesRequest& request : requests) {
    std::vector<BluetoothRemoteGattService*> services =
        GetPrimaryServicesByUUID(device, request.service_uuid);
    switch (request.func) {
      case PrimaryServicesRequest::GET_PRIMARY_SERVICE:
        if (!services.empty()) {
          AddToServicesMapAndSendGetPrimaryServiceSuccess(
              *services[0], request.thread_id, request.request_id);
        } else {
          VLOG(1) << "No service found";
          RecordGetPrimaryServiceOutcome(
              UMAGetPrimaryServiceOutcome::NOT_FOUND);
          Send(new BluetoothMsg_GetPrimaryServiceError(
              request.thread_id, request.request_id,
              WebBluetoothError::SERVICE_NOT_FOUND));
        }
        break;
      case PrimaryServicesRequest::GET_PRIMARY_SERVICES:
        NOTIMPLEMENTED();
        break;
    }
  }
  DCHECK(!ContainsKey(pending_primary_services_requests_, device_address))
      << "Sending get-service responses unexpectedly queued another request.";
}

void BluetoothDispatcherHost::OnRequestDevice(
    int thread_id,
    int request_id,
    int frame_routing_id,
    const std::vector<BluetoothScanFilter>& filters,
    const std::vector<BluetoothUUID>& optional_services) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::REQUEST_DEVICE);
  RecordRequestDeviceArguments(filters, optional_services);

  if (!adapter_.get()) {
    if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
      BluetoothAdapterFactory::GetAdapter(base::Bind(
          &BluetoothDispatcherHost::OnGetAdapter, weak_ptr_on_ui_thread_,
          base::Bind(&BluetoothDispatcherHost::OnRequestDeviceImpl,
                     weak_ptr_on_ui_thread_, thread_id, request_id,
                     frame_routing_id, filters, optional_services)));
      return;
    }
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::NO_BLUETOOTH_ADAPTER);
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id, WebBluetoothError::NO_BLUETOOTH_ADAPTER));
    return;
  }
  OnRequestDeviceImpl(thread_id, request_id, frame_routing_id, filters,
                      optional_services);
}

void BluetoothDispatcherHost::OnGATTServerConnect(
    int thread_id,
    int request_id,
    int frame_routing_id,
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::CONNECT_GATT);
  const base::TimeTicks start_time = base::TimeTicks::Now();

  const CacheQueryResult query_result =
      QueryCacheForDevice(GetOrigin(frame_routing_id), device_id);

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordConnectGATTOutcome(query_result.outcome);
    Send(new BluetoothMsg_GATTServerConnectError(thread_id, request_id,
                                                 query_result.GetWebError()));
    return;
  }

  // If we are already connected no need to connect again.
  if (connected_devices_map_->HasActiveConnection(device_id)) {
    VLOG(1) << "Already connected.";
    Send(new BluetoothMsg_GATTServerConnectSuccess(thread_id, request_id));
    return;
  }

  query_result.device->CreateGattConnection(
      base::Bind(&BluetoothDispatcherHost::OnGATTConnectionCreated,
                 weak_ptr_on_ui_thread_, thread_id, request_id,
                 frame_routing_id, device_id, start_time),
      base::Bind(&BluetoothDispatcherHost::OnCreateGATTConnectionError,
                 weak_ptr_on_ui_thread_, thread_id, request_id, device_id,
                 start_time));
}

void BluetoothDispatcherHost::OnGATTServerDisconnect(
    int thread_id,
    int frame_routing_id,
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::REMOTE_GATT_SERVER_DISCONNECT);

  // Frames can send a disconnect request after they've started navigating,
  // making calls to GetLastCommitted origin invalid. Because we still need
  // to disconnect the device, otherwise we would leave users with no other
  // option to disconnect than closing the tab, we purposefully don't
  // check if the frame has permission to interact with the device.

  // The last BluetoothGattConnection for a device closes the connection when
  // it's destroyed.

  // This only catches disconnections from the renderer. If the device
  // disconnects by itself, or the renderer frame has been deleted
  // due to navigation, we will not hide the indicator.
  // TODO(ortuno): Once we move to Frame and Mojo we will be able
  // to observe the frame's lifetime and hide the indicator when necessary.
  // http://crbug.com/508771
  connected_devices_map_->Remove(frame_routing_id, device_id);
}

void BluetoothDispatcherHost::OnGetPrimaryService(
    int thread_id,
    int request_id,
    int frame_routing_id,
    const std::string& device_id,
    const std::string& service_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::GET_PRIMARY_SERVICE);
  RecordGetPrimaryServiceService(BluetoothUUID(service_uuid));

  if (!allowed_devices_map_.IsOriginAllowedToAccessService(
          GetOrigin(frame_routing_id), device_id, service_uuid)) {
    Send(new BluetoothMsg_GetPrimaryServiceError(
        thread_id, request_id,
        WebBluetoothError::NOT_ALLOWED_TO_ACCESS_SERVICE));
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForDevice(GetOrigin(frame_routing_id), device_id);

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordGetPrimaryServiceOutcome(query_result.outcome);
    Send(new BluetoothMsg_GetPrimaryServiceError(thread_id, request_id,
                                                 query_result.GetWebError()));
    return;
  }

  // There are four possibilities here:
  // 1. Services not discovered and service present in |device|: Send back the
  //    service to the renderer.
  // 2. Services discovered and service present in |device|: Send back the
  //    service to the renderer.
  // 3. Services discovered and service not present in |device|: Send back not
  //    found error.
  // 4. Services not discovered and service not present in |device|: Add request
  //    to map of pending getPrimaryService requests.

  std::vector<BluetoothRemoteGattService*> services =
      GetPrimaryServicesByUUID(query_result.device, service_uuid);

  // 1. & 2.
  if (!services.empty()) {
    VLOG(1) << "Service found in device.";
    const BluetoothRemoteGattService& service = *services[0];
    DCHECK(service.IsPrimary());
    AddToServicesMapAndSendGetPrimaryServiceSuccess(service, thread_id,
                                                    request_id);
    return;
  }

  // 3.
  if (query_result.device->IsGattServicesDiscoveryComplete()) {
    VLOG(1) << "Service not found in device.";
    RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome::NOT_FOUND);
    Send(new BluetoothMsg_GetPrimaryServiceError(
        thread_id, request_id, WebBluetoothError::SERVICE_NOT_FOUND));
    return;
  }

  VLOG(1) << "Adding service request to pending requests.";
  // 4.
  AddToPendingPrimaryServicesRequest(
      query_result.device->GetAddress(),
      PrimaryServicesRequest(thread_id, request_id, service_uuid,
                             PrimaryServicesRequest::GET_PRIMARY_SERVICE));
}

void BluetoothDispatcherHost::OnGetAdapter(
    base::Closure continuation,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_adapter(adapter);
  continuation.Run();
}

void BluetoothDispatcherHost::OnRequestDeviceImpl(
    int thread_id,
    int request_id,
    int frame_routing_id,
    const std::vector<BluetoothScanFilter>& filters,
    const std::vector<BluetoothUUID>& optional_services) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "requestDevice called with the following filters: ";
  for (const BluetoothScanFilter& filter : filters) {
    VLOG(1) << "Name: " << filter.name;
    VLOG(1) << "Name Prefix: " << filter.namePrefix;
    VLOG(1) << "Services:";
    VLOG(1) << "\t[";
    for (const BluetoothUUID& service : filter.services)
      VLOG(1) << "\t\t" << service.value();
    VLOG(1) << "\t]";
  }

  VLOG(1) << "requestDevice called with the following optional services: ";
  for (const BluetoothUUID& service : optional_services)
    VLOG(1) << "\t" << service.value();

  // Check blacklist to reject invalid filters and adjust optional_services.
  if (BluetoothBlacklist::Get().IsExcluded(filters)) {
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLACKLISTED_SERVICE_IN_FILTER);
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id,
        WebBluetoothError::REQUEST_DEVICE_WITH_BLACKLISTED_UUID));
    return;
  }
  std::vector<BluetoothUUID> optional_services_blacklist_filtered(
      optional_services);
  BluetoothBlacklist::Get().RemoveExcludedUuids(
      &optional_services_blacklist_filtered);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, frame_routing_id);
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);

  if (!render_frame_host || !web_contents) {
    DLOG(WARNING) << "Got a requestDevice IPC without a matching "
                  << "RenderFrameHost or WebContents: " << render_process_id_
                  << ", " << frame_routing_id;
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::NO_RENDER_FRAME);
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id,
        WebBluetoothError::REQUEST_DEVICE_WITHOUT_FRAME));
    return;
  }

  const url::Origin requesting_origin =
      render_frame_host->GetLastCommittedOrigin();
  const url::Origin embedding_origin =
      web_contents->GetMainFrame()->GetLastCommittedOrigin();

  // TODO(crbug.com/518042): Enforce correctly-delegated permissions instead of
  // matching origins. When relaxing this, take care to handle non-sandboxed
  // unique origins.
  if (!embedding_origin.IsSameOriginWith(requesting_origin)) {
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id,
        WebBluetoothError::REQUEST_DEVICE_FROM_CROSS_ORIGIN_IFRAME));
    return;
  }
  // The above also excludes unique origins, which are not even same-origin with
  // themselves.
  DCHECK(!requesting_origin.unique());

  DCHECK(adapter_.get());

  if (!adapter_->IsPresent()) {
    VLOG(1) << "Bluetooth Adapter not present. Can't serve requestDevice.";
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLUETOOTH_ADAPTER_NOT_PRESENT);
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id, WebBluetoothError::NO_BLUETOOTH_ADAPTER));
    return;
  }

  // The renderer should never send empty filters.
  if (HasEmptyOrInvalidFilter(filters)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::BDH_EMPTY_OR_INVALID_FILTERS);
    return;
  }

  switch (GetContentClient()->browser()->AllowWebBluetooth(
      web_contents->GetBrowserContext(), requesting_origin, embedding_origin)) {
    case ContentBrowserClient::AllowWebBluetoothResult::BLOCK_POLICY: {
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_POLICY_DISABLED);
      Send(new BluetoothMsg_RequestDeviceError(
          thread_id, request_id,
          WebBluetoothError::CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED));
      return;
    }
    case ContentBrowserClient::AllowWebBluetoothResult::
        BLOCK_GLOBALLY_DISABLED: {
      // Log to the developer console.
      web_contents->GetMainFrame()->AddMessageToConsole(
          content::CONSOLE_MESSAGE_LEVEL_LOG,
          "Bluetooth permission has been blocked.");
      // Block requests.
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_GLOBALLY_DISABLED);
      Send(new BluetoothMsg_RequestDeviceError(
          thread_id, request_id,
          WebBluetoothError::CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED));
      return;
    }
    case ContentBrowserClient::AllowWebBluetoothResult::ALLOW:
      break;
  }

  // Create storage for the information that backs the chooser, and show the
  // chooser.
  RequestDeviceSession* const session =
      new RequestDeviceSession(thread_id, request_id, requesting_origin,
                               filters, optional_services_blacklist_filtered);
  int chooser_id = request_device_sessions_.Add(session);

  BluetoothChooser::EventHandler chooser_event_handler =
      base::Bind(&BluetoothDispatcherHost::OnBluetoothChooserEvent,
                 weak_ptr_on_ui_thread_, chooser_id);
  if (WebContentsDelegate* delegate = web_contents->GetDelegate()) {
    session->chooser =
        delegate->RunBluetoothChooser(render_frame_host, chooser_event_handler);
  }
  if (!session->chooser) {
    LOG(WARNING)
        << "No Bluetooth chooser implementation; falling back to first device.";
    session->chooser.reset(
        new FirstDeviceBluetoothChooser(chooser_event_handler));
  }

  if (!session->chooser->CanAskForScanningPermission()) {
    VLOG(1) << "Closing immediately because Chooser cannot obtain permission.";
    OnBluetoothChooserEvent(chooser_id,
                            BluetoothChooser::Event::DENIED_PERMISSION, "");
    return;
  }

  // Populate the initial list of devices.
  VLOG(1) << "Populating " << adapter_->GetDevices().size()
          << " devices in chooser " << chooser_id;
  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    VLOG(1) << "\t" << device->GetAddress();
    session->AddFilteredDevice(*device);
  }

  if (!session->chooser) {
    // If the dialog's closing, no need to do any of the rest of this.
    return;
  }

  if (!adapter_->IsPowered()) {
    session->chooser->SetAdapterPresence(
        BluetoothChooser::AdapterPresence::POWERED_OFF);
    return;
  }

  StartDeviceDiscovery(session, chooser_id);
}

void BluetoothDispatcherHost::OnDiscoverySessionStarted(
    int chooser_id,
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "Started discovery session for " << chooser_id;
  if (RequestDeviceSession* session =
          request_device_sessions_.Lookup(chooser_id)) {
    session->discovery_session = std::move(discovery_session);

    // Arrange to stop discovery later.
    discovery_session_timer_.Reset();
  } else {
    VLOG(1) << "Chooser " << chooser_id
            << " was closed before the session finished starting. Stopping.";
    StopDiscoverySession(std::move(discovery_session));
  }
}

void BluetoothDispatcherHost::OnDiscoverySessionStartedError(int chooser_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "Failed to start discovery session for " << chooser_id;
  if (RequestDeviceSession* session =
          request_device_sessions_.Lookup(chooser_id)) {
    if (session->chooser && !session->discovery_session) {
      session->chooser->ShowDiscoveryState(
          BluetoothChooser::DiscoveryState::FAILED_TO_START);
    }
  }
  // Ignore discovery session start errors when the dialog was already closed by
  // the time they happen.
}

void BluetoothDispatcherHost::OnBluetoothChooserEvent(
    int chooser_id,
    BluetoothChooser::Event event,
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RequestDeviceSession* session = request_device_sessions_.Lookup(chooser_id);
  DCHECK(session) << "Shouldn't receive an event (" << static_cast<int>(event)
                  << ") from a closed chooser.";
  CHECK(session->chooser) << "Shouldn't receive an event ("
                          << static_cast<int>(event)
                          << ") from a closed chooser.";
  switch (event) {
    case BluetoothChooser::Event::RESCAN:
      StartDeviceDiscovery(session, chooser_id);
      // No need to close the chooser so we return.
      return;
    case BluetoothChooser::Event::DENIED_PERMISSION:
    case BluetoothChooser::Event::CANCELLED:
    case BluetoothChooser::Event::SELECTED:
      break;
    case BluetoothChooser::Event::SHOW_OVERVIEW_HELP:
      VLOG(1) << "Overview Help link pressed.";
      break;
    case BluetoothChooser::Event::SHOW_ADAPTER_OFF_HELP:
      VLOG(1) << "Adapter Off Help link pressed.";
      break;
    case BluetoothChooser::Event::SHOW_NEED_LOCATION_HELP:
      VLOG(1) << "Need Location Help link pressed.";
      break;
  }

  // Synchronously ensure nothing else calls into the chooser after it has
  // asked to be closed.
  session->chooser.reset();

  // Yield to the event loop to make sure we don't destroy the session
  // within a BluetoothDispatcherHost stack frame.
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&BluetoothDispatcherHost::FinishClosingChooser,
                     weak_ptr_on_ui_thread_, chooser_id, event, device_id))) {
    LOG(WARNING) << "No TaskRunner; not closing requestDevice dialog.";
  }
}

void BluetoothDispatcherHost::FinishClosingChooser(
    int chooser_id,
    BluetoothChooser::Event event,
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RequestDeviceSession* session = request_device_sessions_.Lookup(chooser_id);
  DCHECK(session) << "Session removed unexpectedly.";

  if ((event != BluetoothChooser::Event::DENIED_PERMISSION) &&
      (event != BluetoothChooser::Event::SELECTED)) {
    RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
    Send(new BluetoothMsg_RequestDeviceError(
        session->thread_id, session->request_id,
        WebBluetoothError::CHOOSER_CANCELLED));
    request_device_sessions_.Remove(chooser_id);
    return;
  }
  if (event == BluetoothChooser::Event::DENIED_PERMISSION) {
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_DENIED_PERMISSION);
    VLOG(1) << "Bluetooth chooser denied permission";
    Send(new BluetoothMsg_RequestDeviceError(
        session->thread_id, session->request_id,
        WebBluetoothError::CHOOSER_NOT_SHOWN_USER_DENIED_PERMISSION_TO_SCAN));
    request_device_sessions_.Remove(chooser_id);
    return;
  }
  DCHECK_EQ(static_cast<int>(event),
            static_cast<int>(BluetoothChooser::Event::SELECTED));

  // |device_id| is the Device Address that RequestDeviceSession passed to
  // chooser->AddDevice().
  const device::BluetoothDevice* const device = adapter_->GetDevice(device_id);
  if (device == nullptr) {
    VLOG(1) << "Device " << device_id << " no longer in adapter";
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::CHOSEN_DEVICE_VANISHED);
    Send(new BluetoothMsg_RequestDeviceError(
        session->thread_id, session->request_id,
        WebBluetoothError::CHOSEN_DEVICE_VANISHED));
    request_device_sessions_.Remove(chooser_id);
    return;
  }

  const std::string& device_id_for_origin = allowed_devices_map_.AddDevice(
      session->origin, device->GetAddress(), session->filters,
      session->optional_services);

  VLOG(1) << "Device: " << device->GetName();
  VLOG(1) << "UUIDs: ";

  device::BluetoothDevice::UUIDList filtered_uuids;
  for (BluetoothUUID uuid : device->GetUUIDs()) {
    if (allowed_devices_map_.IsOriginAllowedToAccessService(
            session->origin, device_id_for_origin, uuid.canonical_value())) {
      VLOG(1) << "\t Allowed: " << uuid.canonical_value();
      filtered_uuids.push_back(uuid);
    } else {
      VLOG(1) << "\t Not Allowed: " << uuid.canonical_value();
    }
  }

  content::BluetoothDevice device_ipc(
      device_id_for_origin,  // id
      device->GetName(),     // name
      content::BluetoothDevice::ValidatePower(
          device->GetInquiryTxPower()),  // tx_power
      content::BluetoothDevice::ValidatePower(
          device->GetInquiryRSSI()),  // rssi
      device->GetBluetoothClass(),    // device_class
      device->GetVendorIDSource(),    // vendor_id_source
      device->GetVendorID(),          // vendor_id
      device->GetProductID(),         // product_id
      device->GetDeviceID(),          // product_version
      content::BluetoothDevice::UUIDsFromBluetoothUUIDs(
          filtered_uuids));  // uuids
  RecordRequestDeviceOutcome(UMARequestDeviceOutcome::SUCCESS);
  Send(new BluetoothMsg_RequestDeviceSuccess(session->thread_id,
                                             session->request_id, device_ipc));
  request_device_sessions_.Remove(chooser_id);
}

void BluetoothDispatcherHost::OnGATTConnectionCreated(
    int thread_id,
    int request_id,
    int frame_routing_id,
    const std::string& device_id,
    base::TimeTicks start_time,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordConnectGATTTimeSuccess(base::TimeTicks::Now() - start_time);
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::SUCCESS);
  connected_devices_map_->InsertOrReplace(frame_routing_id, device_id,
                                          std::move(connection));
  Send(new BluetoothMsg_GATTServerConnectSuccess(thread_id, request_id));
}

void BluetoothDispatcherHost::OnCreateGATTConnectionError(
    int thread_id,
    int request_id,
    const std::string& device_id,
    base::TimeTicks start_time,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // There was an error creating the ATT Bearer so we reject with
  // NetworkError.
  // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothdevice-connectgatt
  RecordConnectGATTTimeFailed(base::TimeTicks::Now() - start_time);
  // RecordConnectGATTOutcome is called by TranslateConnectError.
  Send(new BluetoothMsg_GATTServerConnectError(
      thread_id, request_id, TranslateConnectError(error_code)));
}

void BluetoothDispatcherHost::AddToServicesMapAndSendGetPrimaryServiceSuccess(
    const device::BluetoothRemoteGattService& service,
    int thread_id,
    int request_id) {
  const std::string& service_identifier = service.GetIdentifier();
  const std::string& device_address = service.GetDevice()->GetAddress();
  auto insert_result =
      service_to_device_.insert(make_pair(service_identifier, device_address));

  // If a value is already in map, DCHECK it's valid.
  if (!insert_result.second)
    DCHECK_EQ(insert_result.first->second, device_address);

  RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome::SUCCESS);
  Send(new BluetoothMsg_GetPrimaryServiceSuccess(thread_id, request_id,
                                                 service_identifier));
}

CacheQueryResult BluetoothDispatcherHost::QueryCacheForDevice(
    const url::Origin& origin,
    const std::string& device_id) {
  const std::string& device_address =
      allowed_devices_map_.GetDeviceAddress(origin, device_id);
  if (device_address.empty()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result;
  result.device = adapter_->GetDevice(device_address);

  // When a device can't be found in the BluetoothAdapter, that generally
  // indicates that it's gone out of range. We reject with a NetworkError in
  // that case.
  // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothdevice-connectgatt
  if (result.device == nullptr) {
    result.outcome = CacheQueryOutcome::NO_DEVICE;
  }
  return result;
}

CacheQueryResult BluetoothDispatcherHost::QueryCacheForService(
    const url::Origin& origin,
    const std::string& service_instance_id) {
  auto device_iter = service_to_device_.find(service_instance_id);

  // Kill the renderer, see "ID Not In Map Note" above.
  if (device_iter == service_to_device_.end()) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_INVALID_SERVICE_ID);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  const std::string& device_id =
      allowed_devices_map_.GetDeviceId(origin, device_iter->second);
  // Kill the renderer if the origin is not allowed to access the device.
  if (device_id.empty()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result = QueryCacheForDevice(origin, device_id);

  if (result.outcome != CacheQueryOutcome::SUCCESS) {
    return result;
  }

  result.service = result.device->GetGattService(service_instance_id);
  if (result.service == nullptr) {
    result.outcome = CacheQueryOutcome::NO_SERVICE;
  } else if (!allowed_devices_map_.IsOriginAllowedToAccessService(
                 origin, device_id,
                 result.service->GetUUID().canonical_value())) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_SERVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }
  return result;
}

void BluetoothDispatcherHost::AddAdapterObserver(
    device::BluetoothAdapter::Observer* observer) {
  adapter_observers_.insert(observer);
  if (adapter_) {
    adapter_->AddObserver(observer);
  }
}

void BluetoothDispatcherHost::RemoveAdapterObserver(
    device::BluetoothAdapter::Observer* observer) {
  DCHECK(adapter_observers_.erase(observer));
  if (adapter_) {
    adapter_->RemoveObserver(observer);
  }
}

void BluetoothDispatcherHost::AddToPendingPrimaryServicesRequest(
    const std::string& device_address,
    const PrimaryServicesRequest& request) {
  pending_primary_services_requests_[device_address].push_back(request);
}

url::Origin BluetoothDispatcherHost::GetOrigin(int frame_routing_id) {
  return RenderFrameHostImpl::FromID(render_process_id_, frame_routing_id)
      ->GetLastCommittedOrigin();
}

}  // namespace content
