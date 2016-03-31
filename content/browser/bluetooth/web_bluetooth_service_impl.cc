// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <vector>

#include "content/browser/bluetooth/bluetooth_blacklist.h"
#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_frame_host.h"
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
    : render_frame_host_(render_frame_host),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebBluetoothServiceImpl::~WebBluetoothServiceImpl() {}

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

}  // namespace content
