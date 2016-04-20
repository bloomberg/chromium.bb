// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include "base/thread_task_runner_handle.h"
#include "content/browser/bluetooth/bluetooth_blacklist.h"
#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

using device::BluetoothGattService;

namespace content {

namespace {

blink::mojom::WebBluetoothError TranslateGATTErrorAndRecord(
    BluetoothGattService::GattErrorCode error_code,
    UMAGATTOperation operation) {
  switch (error_code) {
    case BluetoothGattService::GATT_ERROR_UNKNOWN:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::UNKNOWN);
      return blink::mojom::WebBluetoothError::GATT_UNKNOWN_ERROR;
    case BluetoothGattService::GATT_ERROR_FAILED:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::FAILED);
      return blink::mojom::WebBluetoothError::GATT_UNKNOWN_FAILURE;
    case BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::IN_PROGRESS);
      return blink::mojom::WebBluetoothError::GATT_OPERATION_IN_PROGRESS;
    case BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::INVALID_LENGTH);
      return blink::mojom::WebBluetoothError::GATT_INVALID_ATTRIBUTE_LENGTH;
    case BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PERMITTED);
      return blink::mojom::WebBluetoothError::GATT_NOT_PERMITTED;
    case BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_AUTHORIZED);
      return blink::mojom::WebBluetoothError::GATT_NOT_AUTHORIZED;
    case BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PAIRED);
      return blink::mojom::WebBluetoothError::GATT_NOT_PAIRED;
    case BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_SUPPORTED);
      return blink::mojom::WebBluetoothError::GATT_NOT_SUPPORTED;
  }
  NOTREACHED();
  return blink::mojom::WebBluetoothError::GATT_UNTRANSLATED_ERROR_CODE;
}

}  // namespace

using CacheQueryResult = BluetoothDispatcherHost::CacheQueryResult;

WebBluetoothServiceImpl::WebBluetoothServiceImpl(
    RenderFrameHost* render_frame_host,
    blink::mojom::WebBluetoothServiceRequest request)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents());

  GetBluetoothDispatcherHost()->AddAdapterObserver(this);
}

WebBluetoothServiceImpl::~WebBluetoothServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetBluetoothDispatcherHost()->RemoveAdapterObserver(this);
}

void WebBluetoothServiceImpl::SetClientConnectionErrorHandler(
    base::Closure closure) {
  binding_.set_connection_error_handler(closure);
}

void WebBluetoothServiceImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      navigation_handle->GetRenderFrameHost() == render_frame_host_ &&
      !navigation_handle->IsSamePage()) {
    // After navigation we need to clear the frame's state.
    ClearState();
  }
}

void WebBluetoothServiceImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  if (!present) {
    ClearState();
  }
}

void WebBluetoothServiceImpl::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  // TODO(ortuno): Only send characteristic value changed events for
  // characteristics that we've returned in the past. We can't yet do
  // this because WebBluetoothServiceImpl doesn't have direct access
  // to the list of returned characteristics.
  // https://crbug.com/508771
  if (BluetoothBlacklist::Get().IsExcluded(characteristic->GetUUID())) {
    return;
  }

  // On Chrome OS and Linux, GattCharacteristicValueChanged is called before the
  // success callback for ReadRemoteCharacteristic is called, which could result
  // in an event being fired before the readValue promise is resolved.
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&WebBluetoothServiceImpl::NotifyCharacteristicValueChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     characteristic->GetIdentifier(), value))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

void WebBluetoothServiceImpl::NotifyCharacteristicValueChanged(
    const std::string& characteristic_instance_id,
    std::vector<uint8_t> value) {
  if (client_) {
    client_->RemoteCharacteristicValueChanged(
        characteristic_instance_id, mojo::Array<uint8_t>(std::move(value)));
  }
}

void WebBluetoothServiceImpl::SetClient(
    blink::mojom::WebBluetoothServiceClientAssociatedPtrInfo client) {
  DCHECK(!client_.get());
  client_.Bind(std::move(client));
}

void WebBluetoothServiceImpl::RemoteCharacteristicReadValue(
    const mojo::String& characteristic_instance_id,
    const RemoteCharacteristicReadValueCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_READ_VALUE);

  const CacheQueryResult query_result =
      GetBluetoothDispatcherHost()->QueryCacheForCharacteristic(
          GetOrigin(), characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordCharacteristicReadValueOutcome(query_result.outcome);
    callback.Run(query_result.GetWebError(), nullptr /* value */);
    return;
  }

  if (BluetoothBlacklist::Get().IsExcludedFromReads(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::BLACKLISTED);
    callback.Run(blink::mojom::WebBluetoothError::BLACKLISTED_READ,
                 nullptr /* value */);
    return;
  }

  query_result.characteristic->ReadRemoteCharacteristic(
      base::Bind(&WebBluetoothServiceImpl::OnReadValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnReadValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicWriteValue(
    const mojo::String& characteristic_instance_id,
    mojo::Array<uint8_t> value,
    const RemoteCharacteristicWriteValueCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_WRITE_VALUE);

  // We perform the length check on the renderer side. So if we
  // get a value with length > 512, we can assume it's a hostile
  // renderer and kill it.
  if (value.size() > 512) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  const CacheQueryResult query_result =
      GetBluetoothDispatcherHost()->QueryCacheForCharacteristic(
          GetOrigin(), characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    binding_.Close();
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordCharacteristicWriteValueOutcome(query_result.outcome);
    callback.Run(query_result.GetWebError());
    return;
  }

  if (BluetoothBlacklist::Get().IsExcludedFromWrites(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::BLACKLISTED);
    callback.Run(blink::mojom::WebBluetoothError::BLACKLISTED_WRITE);
    return;
  }

  query_result.characteristic->WriteRemoteCharacteristic(
      value.To<std::vector<uint8_t>>(),
      base::Bind(&WebBluetoothServiceImpl::OnWriteValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnWriteValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStartNotifications(
    const mojo::String& characteristic_instance_id,
    const RemoteCharacteristicStartNotificationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_START_NOTIFICATIONS);

  auto iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (iter != characteristic_id_to_notify_session_.end() &&
      iter->second->IsActive()) {
    // If the frame has already started notifications and the notifications
    // are active we return SUCCESS.
    callback.Run(blink::mojom::WebBluetoothError::SUCCESS);
    return;
  }

  const CacheQueryResult query_result =
      GetBluetoothDispatcherHost()->QueryCacheForCharacteristic(
          GetOrigin(), characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    binding_.Close();
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordStartNotificationsOutcome(query_result.outcome);
    callback.Run(query_result.GetWebError());
    return;
  }

  device::BluetoothGattCharacteristic::Properties notify_or_indicate =
      query_result.characteristic->GetProperties() &
      (device::BluetoothGattCharacteristic::PROPERTY_NOTIFY |
       device::BluetoothGattCharacteristic::PROPERTY_INDICATE);
  if (!notify_or_indicate) {
    callback.Run(blink::mojom::WebBluetoothError::GATT_NOT_SUPPORTED);
    return;
  }

  query_result.characteristic->StartNotifySession(
      base::Bind(&WebBluetoothServiceImpl::OnStartNotifySessionSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&WebBluetoothServiceImpl::OnStartNotifySessionFailed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStopNotifications(
    const mojo::String& characteristic_instance_id,
    const RemoteCharacteristicStopNotificationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_STOP_NOTIFICATIONS);

  const CacheQueryResult query_result =
      GetBluetoothDispatcherHost()->QueryCacheForCharacteristic(
          GetOrigin(), characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    binding_.Close();
    return;
  }

  auto notify_session_iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (notify_session_iter == characteristic_id_to_notify_session_.end()) {
    // If the frame hasn't subscribed to notifications before we just
    // run the callback.
    callback.Run();
    return;
  }
  notify_session_iter->second->Stop(base::Bind(
      &WebBluetoothServiceImpl::OnStopNotifySessionComplete,
      weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id, callback));
}

void WebBluetoothServiceImpl::OnReadValueSuccess(
    const RemoteCharacteristicReadValueCallback& callback,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  callback.Run(blink::mojom::WebBluetoothError::SUCCESS,
               mojo::Array<uint8_t>::From(value));
}

void WebBluetoothServiceImpl::OnReadValueFailed(
    const RemoteCharacteristicReadValueCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(TranslateGATTErrorAndRecord(
                   error_code, UMAGATTOperation::CHARACTERISTIC_READ),
               nullptr /* value */);
}

void WebBluetoothServiceImpl::OnWriteValueSuccess(
    const RemoteCharacteristicWriteValueCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  callback.Run(blink::mojom::WebBluetoothError::SUCCESS);
}

void WebBluetoothServiceImpl::OnWriteValueFailed(
    const RemoteCharacteristicWriteValueCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::CHARACTERISTIC_WRITE));
}

void WebBluetoothServiceImpl::OnStartNotifySessionSuccess(
    const RemoteCharacteristicStartNotificationsCallback& callback,
    std::unique_ptr<device::BluetoothGattNotifySession> notify_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Copy Characteristic Instance ID before passing a unique pointer because
  // compilers may evaluate arguments in any order.
  std::string characteristic_instance_id =
      notify_session->GetCharacteristicIdentifier();
  // Saving the BluetoothGattNotifySession keeps notifications active.
  characteristic_id_to_notify_session_[characteristic_instance_id] =
      std::move(notify_session);
  callback.Run(blink::mojom::WebBluetoothError::SUCCESS);
}

void WebBluetoothServiceImpl::OnStartNotifySessionFailed(
    const RemoteCharacteristicStartNotificationsCallback& callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::START_NOTIFICATIONS));
}

void WebBluetoothServiceImpl::OnStopNotifySessionComplete(
    const std::string& characteristic_instance_id,
    const RemoteCharacteristicStopNotificationsCallback& callback) {
  characteristic_id_to_notify_session_.erase(characteristic_instance_id);
  callback.Run();
}

RenderProcessHost* WebBluetoothServiceImpl::GetRenderProcessHost() {
  return render_frame_host_->GetProcess();
}

BluetoothDispatcherHost* WebBluetoothServiceImpl::GetBluetoothDispatcherHost() {
  RenderProcessHostImpl* render_process_host_impl =
      static_cast<RenderProcessHostImpl*>(GetRenderProcessHost());
  return render_process_host_impl->GetBluetoothDispatcherHost();
}

void WebBluetoothServiceImpl::CrashRendererAndClosePipe(
    bad_message::BadMessageReason reason) {
  bad_message::ReceivedBadMessage(GetRenderProcessHost(), reason);
  binding_.Close();
}

url::Origin WebBluetoothServiceImpl::GetOrigin() {
  return render_frame_host_->GetLastCommittedOrigin();
}

void WebBluetoothServiceImpl::ClearState() {
  characteristic_id_to_notify_session_.clear();
}

}  // namespace content
