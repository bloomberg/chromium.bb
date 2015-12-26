// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/usb/web_usb_client_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/move.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/mojo/type_converters.h"
#include "content/child/scoped_web_callbacks.h"
#include "content/public/common/service_registry.h"
#include "content/renderer/usb/type_converters.h"
#include "content/renderer/usb/web_usb_device_impl.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebPassOwnPtr.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceFilter.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceRequestOptions.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBError.h"

namespace content {

namespace {

const char kNoServiceError[] = "USB service unavailable.";

// Generic default rejection handler for any WebUSB callbacks type. Assumes
// |CallbacksType| is a blink::WebCallbacks<T, const blink::WebUSBError&>
// for any type |T|.
template <typename CallbacksType>
void RejectCallbacksWithError(const blink::WebUSBError& error,
                              scoped_ptr<CallbacksType> callbacks) {
  callbacks->onError(error);
}

// Create a new ScopedWebCallbacks for WebUSB client callbacks, defaulting to
// a "no service" rejection.
template <typename CallbacksType>
ScopedWebCallbacks<CallbacksType> MakeScopedUSBCallbacks(
    CallbacksType* callbacks) {
  return make_scoped_web_callbacks(
      callbacks,
      base::Bind(&RejectCallbacksWithError<CallbacksType>,
                 blink::WebUSBError(blink::WebUSBError::Error::NotFound,
                                    base::ASCIIToUTF16(kNoServiceError))));
}

void OnGetDevicesComplete(
    ScopedWebCallbacks<blink::WebUSBClientGetDevicesCallbacks> scoped_callbacks,
    device::usb::DeviceManager* device_manager,
    mojo::Array<device::usb::DeviceInfoPtr> results) {
  blink::WebVector<blink::WebUSBDevice*>* devices =
      new blink::WebVector<blink::WebUSBDevice*>(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    device::usb::DevicePtr device;
    device_manager->GetDevice(results[i]->guid, mojo::GetProxy(&device));
    (*devices)[i] = new WebUSBDeviceImpl(
        std::move(device),
        mojo::ConvertTo<blink::WebUSBDeviceInfo>(results[i]));
  }
  scoped_callbacks.PassCallbacks()->onSuccess(blink::adoptWebPtr(devices));
}

void OnRequestDevicesComplete(
    ScopedWebCallbacks<blink::WebUSBClientRequestDeviceCallbacks> callbacks,
    device::usb::DeviceManager* device_manager,
    device::usb::DeviceInfoPtr result) {
  auto scoped_callbacks = callbacks.PassCallbacks();
  if (result) {
    device::usb::DevicePtr device;
    device_manager->GetDevice(result->guid, mojo::GetProxy(&device));
    blink::WebUSBDevice* web_usb_device = new WebUSBDeviceImpl(
        std::move(device), mojo::ConvertTo<blink::WebUSBDeviceInfo>(result));

    scoped_callbacks->onSuccess(blink::adoptWebPtr(web_usb_device));
  } else {
    scoped_callbacks->onError(
        blink::WebUSBError(blink::WebUSBError::Error::NotFound,
                           base::ASCIIToUTF16("No device selected.")));
  }
}

}  // namespace

WebUSBClientImpl::WebUSBClientImpl(content::ServiceRegistry* service_registry)
    : service_registry_(service_registry) {}

WebUSBClientImpl::~WebUSBClientImpl() {}

void WebUSBClientImpl::getDevices(
    blink::WebUSBClientGetDevicesCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  GetDeviceManager()->GetDevices(
      nullptr,
      base::Bind(&OnGetDevicesComplete, base::Passed(&scoped_callbacks),
                 base::Unretained(device_manager_.get())));
}

void WebUSBClientImpl::requestDevice(
    const blink::WebUSBDeviceRequestOptions& options,
    blink::WebUSBClientRequestDeviceCallbacks* callbacks) {
  if (!webusb_permission_bubble_) {
    service_registry_->ConnectToRemoteService(
        mojo::GetProxy(&webusb_permission_bubble_));
  }

  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);

  mojo::Array<device::usb::DeviceFilterPtr> device_filters =
      mojo::Array<device::usb::DeviceFilterPtr>::From(options.filters);

  webusb_permission_bubble_->GetPermission(
      std::move(device_filters),
      base::Bind(&OnRequestDevicesComplete, base::Passed(&scoped_callbacks),
                 base::Unretained(device_manager_.get())));
}

void WebUSBClientImpl::setObserver(Observer* observer) {
  if (!observer_) {
    // Set up two sequential calls to GetDeviceChanges to avoid latency.
    device::usb::DeviceManager* device_manager = GetDeviceManager();
    device_manager->GetDeviceChanges(base::Bind(
        &WebUSBClientImpl::OnDeviceChangeNotification, base::Unretained(this)));
    device_manager->GetDeviceChanges(base::Bind(
        &WebUSBClientImpl::OnDeviceChangeNotification, base::Unretained(this)));
  }

  observer_ = observer;
}

device::usb::DeviceManager* WebUSBClientImpl::GetDeviceManager() {
  if (!device_manager_)
    service_registry_->ConnectToRemoteService(mojo::GetProxy(&device_manager_));
  return device_manager_.get();
}

void WebUSBClientImpl::OnDeviceChangeNotification(
    device::usb::DeviceChangeNotificationPtr notification) {
  if (!observer_)
    return;

  device_manager_->GetDeviceChanges(base::Bind(
      &WebUSBClientImpl::OnDeviceChangeNotification, base::Unretained(this)));
  for (size_t i = 0; i < notification->devices_added.size(); ++i) {
    const device::usb::DeviceInfoPtr& device_info =
        notification->devices_added[i];
    device::usb::DevicePtr device;
    device_manager_->GetDevice(device_info->guid, mojo::GetProxy(&device));
    observer_->onDeviceConnected(blink::adoptWebPtr(new WebUSBDeviceImpl(
        std::move(device),
        mojo::ConvertTo<blink::WebUSBDeviceInfo>(device_info))));
  }
  for (size_t i = 0; i < notification->devices_removed.size(); ++i) {
    const device::usb::DeviceInfoPtr& device_info =
        notification->devices_removed[i];
    device::usb::DevicePtr device;
    device_manager_->GetDevice(device_info->guid, mojo::GetProxy(&device));
    observer_->onDeviceDisconnected(blink::adoptWebPtr(new WebUSBDeviceImpl(
        std::move(device),
        mojo::ConvertTo<blink::WebUSBDeviceInfo>(device_info))));
  }
}

}  // namespace content
