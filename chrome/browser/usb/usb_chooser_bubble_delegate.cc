// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_bubble_delegate.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "components/bubble/bubble_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/core/device_client.h"
#include "device/devices_app/usb/type_converters.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "url/gurl.h"

namespace {

// Check if the origin is in the description set.
bool FindOriginInDescriptorSet(const device::WebUsbDescriptorSet* set,
                               const GURL& origin) {
  if (!set)
    return false;

  if (ContainsValue(set->origins, origin))
    return true;

  for (const auto& config : set->configurations) {
    if (ContainsValue(config.origins, origin))
      return true;

    for (const auto& function : config.functions) {
      if (ContainsValue(function.origins, origin))
        return true;
    }
  }

  return false;
}

}  // namespace

UsbChooserBubbleDelegate::UsbChooserBubbleDelegate(
    Browser* browser,
    mojo::Array<device::usb::DeviceFilterPtr> device_filters,
    content::RenderFrameHost* render_frame_host,
    const webusb::WebUsbPermissionBubble::GetPermissionCallback& callback)
    : ChooserBubbleDelegate(browser),
      render_frame_host_(render_frame_host),
      callback_(callback),
      usb_service_observer_(this),
      weak_factory_(this) {
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  if (!usb_service)
    return;

  if (!usb_service_observer_.IsObserving(usb_service))
    usb_service_observer_.Add(usb_service);

  if (!device_filters.is_null())
    filters_ = device_filters.To<std::vector<device::UsbDeviceFilter>>();

  usb_service->GetDevices(base::Bind(
      &UsbChooserBubbleDelegate::GotUsbDeviceList, weak_factory_.GetWeakPtr()));
}

UsbChooserBubbleDelegate::~UsbChooserBubbleDelegate() {
  if (!callback_.is_null())
    callback_.Run(nullptr);
}

const std::vector<base::string16>& UsbChooserBubbleDelegate::GetOptions()
    const {
  return devices_names_;
}

void UsbChooserBubbleDelegate::Select(int index) {
  size_t idx = static_cast<size_t>(index);
  size_t num_options = devices_.size();
  DCHECK_LT(idx, num_options);
  if (idx < num_options) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host_);
    GURL embedding_origin =
        web_contents->GetMainFrame()->GetLastCommittedURL().GetOrigin();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    UsbChooserContext* chooser_context =
        UsbChooserContextFactory::GetForProfile(profile);
    chooser_context->GrantDevicePermission(
        render_frame_host_->GetLastCommittedURL().GetOrigin(), embedding_origin,
        devices_[idx]->guid());

    device::usb::DeviceInfoPtr device_info_ptr =
        device::usb::DeviceInfo::From(*devices_[idx]);
    callback_.Run(device_info_ptr.Pass());
  } else {
    callback_.Run(nullptr);
  }
  callback_.reset();  // Reset |callback_| so that it is only run once.

  if (bubble_controller_)
    bubble_controller_->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
}

void UsbChooserBubbleDelegate::Cancel() {
  if (bubble_controller_)
    bubble_controller_->CloseBubble(BUBBLE_CLOSE_CANCELED);
}

void UsbChooserBubbleDelegate::Close() {}

void UsbChooserBubbleDelegate::OnDeviceAdded(
    scoped_refptr<device::UsbDevice> device) {
  DCHECK(!ContainsValue(devices_, device));
  if (device::UsbDeviceFilter::MatchesAny(device, filters_) &&
      FindOriginInDescriptorSet(
          device->webusb_allowed_origins(),
          render_frame_host_->GetLastCommittedURL().GetOrigin())) {
    devices_.push_back(device);
    devices_names_.push_back(device->product_string());
    if (observer())
      observer()->OnOptionAdded(static_cast<int>(devices_names_.size()) - 1);
  }
}

void UsbChooserBubbleDelegate::OnDeviceRemoved(
    scoped_refptr<device::UsbDevice> device) {
  auto iter = std::find(devices_.begin(), devices_.end(), device);
  if (iter != devices_.end()) {
    int index = iter - devices_.begin();
    devices_.erase(iter);
    devices_names_.erase(devices_names_.begin() + index);
    if (observer())
      observer()->OnOptionRemoved(index);
  }
}

// Get a list of devices that can be shown in the chooser bubble UI for
// user to grant permsssion.
void UsbChooserBubbleDelegate::GotUsbDeviceList(
    const std::vector<scoped_refptr<device::UsbDevice>>& devices) {
  for (const auto& device : devices) {
    if (device::UsbDeviceFilter::MatchesAny(device, filters_) &&
        FindOriginInDescriptorSet(
            device->webusb_allowed_origins(),
            render_frame_host_->GetLastCommittedURL().GetOrigin())) {
      devices_.push_back(device);
      devices_names_.push_back(device->product_string());
    }
  }
  if (observer())
    observer()->OnOptionsInitialized();
}

void UsbChooserBubbleDelegate::set_bubble_controller(
    BubbleReference bubble_controller) {
  bubble_controller_ = bubble_controller;
}
