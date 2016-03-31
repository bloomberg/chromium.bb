// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_

#include "base/macros.h"
#include "content/browser/bad_message.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BluetoothDispatcherHost;
class RenderFrameHost;
class RenderProcessHost;

// Implementation of Mojo WebBluetoothService located in
// third_party/WebKit/public/platform/modules/bluetooth.
// It handles Web Bluetooth API requests coming from Blink / renderer
// process and uses the platform abstraction of device/bluetooth.
// WebBluetoothServiceImpl is not thread-safe and should be created on the
// UI thread as required by device/bluetooth.
// This class is instantiated on-demand via Mojo's ConnectToRemoteService
// from the renderer when the first Web Bluetooth API request is handled.
// RenderFrameHostImpl will create an instance of this class and keep
// ownership of it.
class WebBluetoothServiceImpl : public blink::mojom::WebBluetoothService {
 public:
  // |render_frame_host|: The RFH that owns this instance.
  // |request|: The instance will be bound to this request's pipe.
  WebBluetoothServiceImpl(RenderFrameHost* render_frame_host,
                          blink::mojom::WebBluetoothServiceRequest request);
  ~WebBluetoothServiceImpl() override;

 private:
  // WebBluetoothService methods:
  void RemoteCharacteristicWriteValue(
      const mojo::String& characteristic_instance_id,
      mojo::Array<uint8_t> value,
      const RemoteCharacteristicWriteValueCallback& callback) override;

  // Callbacks for BluetoothGattCharacteristic::WriteRemoteCharacteristic.
  void OnWriteValueSuccess(
      const RemoteCharacteristicWriteValueCallback& callback);
  void OnWriteValueFailed(
      const RemoteCharacteristicWriteValueCallback& callback,
      device::BluetoothGattService::GattErrorCode error_code);

  RenderProcessHost* GetRenderProcessHost();
  BluetoothDispatcherHost* GetBluetoothDispatcherHost();
  void CrashRendererAndClosePipe(bad_message::BadMessageReason reason);
  url::Origin GetOrigin();

  // The RFH that owns this instance.
  RenderFrameHost* render_frame_host_;

  // The lifetime of this instance is exclusively managed by the RFH that
  // owns it so we use a "Binding" as opposed to a "StrongBinding" which deletes
  // the service on pipe connection errors.
  mojo::Binding<blink::mojom::WebBluetoothService> binding_;

  base::WeakPtrFactory<WebBluetoothServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebBluetoothServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_
