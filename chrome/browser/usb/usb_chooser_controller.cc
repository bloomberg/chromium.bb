// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_controller.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/core/device_client.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/webusb_descriptors.h"
#include "url/gurl.h"

namespace {

Browser* GetBrowser() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetActiveUserProfile());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

}  // namespace

UsbChooserController::UsbChooserController(
    content::RenderFrameHost* owner,
    mojo::Array<device::usb::DeviceFilterPtr> device_filters,
    content::RenderFrameHost* render_frame_host,
    const device::usb::ChooserService::GetPermissionCallback& callback)
    : ChooserController(owner),
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

  usb_service->GetDevices(base::Bind(&UsbChooserController::GotUsbDeviceList,
                                     weak_factory_.GetWeakPtr()));
}

UsbChooserController::~UsbChooserController() {
  if (!callback_.is_null())
    callback_.Run(nullptr);
}

size_t UsbChooserController::NumOptions() const {
  return devices_.size();
}

const base::string16& UsbChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index].second;
}

void UsbChooserController::Select(size_t index) {
  DCHECK_LT(index, devices_.size());
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
      devices_[index].first->guid());

  device::usb::DeviceInfoPtr device_info_ptr =
      device::usb::DeviceInfo::From(*devices_[index].first);
  callback_.Run(std::move(device_info_ptr));
  callback_.Reset();  // Reset |callback_| so that it is only run once.

  RecordWebUsbChooserClosure(
      devices_[index].first->serial_number().empty()
          ? WEBUSB_CHOOSER_CLOSED_EPHEMERAL_PERMISSION_GRANTED
          : WEBUSB_CHOOSER_CLOSED_PERMISSION_GRANTED);
}

void UsbChooserController::Cancel() {
  RecordWebUsbChooserClosure(devices_.size() == 0
                                 ? WEBUSB_CHOOSER_CLOSED_CANCELLED_NO_DEVICES
                                 : WEBUSB_CHOOSER_CLOSED_CANCELLED);
}

void UsbChooserController::Close() {}

void UsbChooserController::OpenHelpCenterUrl() const {
  GetBrowser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserUsbOverviewURL), content::Referrer(),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initialized */));
}

void UsbChooserController::OnDeviceAdded(
    scoped_refptr<device::UsbDevice> device) {
  if (DisplayDevice(device)) {
    devices_.push_back(std::make_pair(device, device->product_string()));
    if (observer())
      observer()->OnOptionAdded(devices_.size() - 1);
  }
}

void UsbChooserController::OnDeviceRemoved(
    scoped_refptr<device::UsbDevice> device) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->first == device) {
      size_t index = it - devices_.begin();
      devices_.erase(it);
      if (observer())
        observer()->OnOptionRemoved(index);
      return;
    }
  }
}

// Get a list of devices that can be shown in the chooser bubble UI for
// user to grant permsssion.
void UsbChooserController::GotUsbDeviceList(
    const std::vector<scoped_refptr<device::UsbDevice>>& devices) {
  for (const auto& device : devices) {
    if (DisplayDevice(device))
      devices_.push_back(std::make_pair(device, device->product_string()));
  }
  if (observer())
    observer()->OnOptionsInitialized();
}

bool UsbChooserController::DisplayDevice(
    scoped_refptr<device::UsbDevice> device) const {
  return device::UsbDeviceFilter::MatchesAny(device, filters_) &&
         (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableWebUsbSecurity) ||
          device::FindInWebUsbAllowedOrigins(
              device->webusb_allowed_origins(),
              render_frame_host_->GetLastCommittedURL().GetOrigin()));
}
