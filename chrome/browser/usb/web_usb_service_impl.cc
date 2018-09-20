// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "device/base/device_client.h"
#include "device/usb/mojo/device_manager_impl.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/usb_device.h"

// static
bool WebUsbServiceImpl::HasDevicePermission(
    UsbChooserContext* chooser_context,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (UsbBlocklist::Get().IsExcluded(device_info))
    return false;

  return chooser_context->HasDevicePermission(requesting_origin,
                                              embedding_origin, device_info);
}

WebUsbServiceImpl::WebUsbServiceImpl(
    content::RenderFrameHost* render_frame_host,
    base::WeakPtr<WebUsbChooser> usb_chooser)
    : render_frame_host_(render_frame_host),
      usb_chooser_(std::move(usb_chooser)),
      observer_(this),
      weak_factory_(this) {
  // Bind |device_manager_| to UsbDeviceManager and set error handler.
  // TODO(donna.wu@intel.com): Request UsbDeviceManagerPtr from the Device
  // Service after moving //device/usb to //services/device.
  device::usb::DeviceManagerImpl::Create(mojo::MakeRequest(&device_manager_));
  device_manager_.set_connection_error_handler(
      base::BindOnce(&WebUsbServiceImpl::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  bindings_.set_connection_error_handler(base::BindRepeating(
      &WebUsbServiceImpl::OnBindingConnectionError, base::Unretained(this)));
}

WebUsbServiceImpl::~WebUsbServiceImpl() = default;

void WebUsbServiceImpl::BindRequest(
    blink::mojom::WebUsbServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));

  // Listen for add/remove device events from UsbService.
  // TODO(donna.wu@intel.com): Listen to |device_manager_| in the future.
  // We can't set WebUsbServiceImpl as a UsbDeviceManagerClient because
  // the OnDeviceRemoved event will be delivered here after it is delivered
  // to UsbChooserContext, meaning that all ephemeral permission checks in
  // OnDeviceRemoved() will fail.
  if (!observer_.IsObservingSources()) {
    auto* usb_service = device::DeviceClient::Get()->GetUsbService();
    if (usb_service)
      observer_.Add(usb_service);
  }
}

bool WebUsbServiceImpl::HasDevicePermission(
    const device::mojom::UsbDeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host_);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  return HasDevicePermission(
      UsbChooserContextFactory::GetForProfile(profile),
      render_frame_host_->GetLastCommittedURL().GetOrigin(),
      main_frame->GetLastCommittedURL().GetOrigin(), device_info);
}

void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  device_manager_->GetDevices(
      nullptr, base::BindOnce(&WebUsbServiceImpl::OnGetDevices,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebUsbServiceImpl::OnGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list) {
  std::vector<device::mojom::UsbDeviceInfoPtr> device_infos;
  for (auto& device_info : device_info_list) {
    if (HasDevicePermission(*device_info))
      device_infos.push_back(device_info.Clone());
  }
  std::move(callback).Run(std::move(device_infos));
}

void WebUsbServiceImpl::GetDevice(
    const std::string& guid,
    device::mojom::UsbDeviceRequest device_request) {
  // Try to bind with the new device to be created for DeviceOpened/Closed
  // events. It is safe to pass this request directly to |device_manager_|
  // because |guid| is unguessable.
  device::mojom::UsbDeviceClientPtr device_client;
  device_client_bindings_.AddBinding(this, mojo::MakeRequest(&device_client));
  device_manager_->GetDevice(guid, std::move(device_request),
                             std::move(device_client));
}

void WebUsbServiceImpl::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  if (!usb_chooser_)
    std::move(callback).Run(nullptr);

  usb_chooser_->GetPermission(std::move(device_filters), std::move(callback));
}

void WebUsbServiceImpl::SetClient(
    device::mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);

  device::mojom::UsbDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void WebUsbServiceImpl::OnDeviceAdded(scoped_refptr<device::UsbDevice> device) {
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  if (!HasDevicePermission(*device_info))
    return;

  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceAdded(device_info->Clone());
      });
}

void WebUsbServiceImpl::OnDeviceRemoved(
    scoped_refptr<device::UsbDevice> device) {
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  if (!HasDevicePermission(*device_info))
    return;

  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceRemoved(device_info->Clone());
      });
}

// device::mojom::UsbDeviceClient implementation:
void WebUsbServiceImpl::OnDeviceOpened() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->IncrementConnectionCount(render_frame_host_);
}

void WebUsbServiceImpl::OnDeviceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->DecrementConnectionCount(render_frame_host_);
}

void WebUsbServiceImpl::OnDeviceManagerConnectionError() {
  device_manager_.reset();

  // Close the connection with blink.
  clients_.CloseAll();
  bindings_.CloseAllBindings();
  observer_.RemoveAll();
}

void WebUsbServiceImpl::OnBindingConnectionError() {
  if (bindings_.empty())
    observer_.RemoveAll();
}

void WebUsbServiceImpl::WillDestroyUsbService() {
  OnDeviceManagerConnectionError();
}
