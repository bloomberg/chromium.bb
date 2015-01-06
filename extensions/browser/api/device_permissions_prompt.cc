// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_prompt.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_ids.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

using device::UsbDevice;
using device::UsbDeviceFilter;
using device::UsbService;

namespace extensions {

DevicePermissionsPrompt::Prompt::DeviceInfo::DeviceInfo(
    scoped_refptr<UsbDevice> device)
    : device(device) {
  base::string16 manufacturer_string;
  if (device->GetManufacturer(&manufacturer_string)) {
    original_manufacturer_string = manufacturer_string;
  } else {
    const char* vendor_name =
        device::UsbIds::GetVendorName(device->vendor_id());
    if (vendor_name) {
      manufacturer_string = base::UTF8ToUTF16(vendor_name);
    } else {
      base::string16 vendor_id =
          base::ASCIIToUTF16(base::StringPrintf("0x%04x", device->vendor_id()));
      manufacturer_string =
          l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_VENDOR, vendor_id);
    }
  }

  base::string16 product_string;
  if (device->GetProduct(&product_string)) {
    original_product_string = product_string;
  } else {
    const char* product_name = device::UsbIds::GetProductName(
        device->vendor_id(), device->product_id());
    if (product_name) {
      product_string = base::UTF8ToUTF16(product_name);
    } else {
      base::string16 product_id = base::ASCIIToUTF16(
          base::StringPrintf("0x%04x", device->product_id()));
      product_string =
          l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_PRODUCT, product_id);
    }
  }

  if (!device->GetSerialNumber(&serial_number)) {
    serial_number.clear();
  }

  name = l10n_util::GetStringFUTF16(IDS_DEVICE_PERMISSIONS_DEVICE_NAME,
                                    product_string, manufacturer_string);
}

DevicePermissionsPrompt::Prompt::DeviceInfo::~DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::Prompt()
    : extension_(nullptr),
      browser_context_(nullptr),
      multiple_(false),
      observer_(nullptr),
      usb_service_observer_(this) {
}

void DevicePermissionsPrompt::Prompt::SetObserver(Observer* observer) {
  observer_ = observer;

  if (observer_) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&DevicePermissionsPrompt::Prompt::DoDeviceQuery, this));
  }
}

base::string16 DevicePermissionsPrompt::Prompt::GetHeading() const {
  return l10n_util::GetStringUTF16(
      multiple_ ? IDS_DEVICE_PERMISSIONS_PROMPT_TITLE_MULTIPLE
                : IDS_DEVICE_PERMISSIONS_PROMPT_TITLE_SINGLE);
}

base::string16 DevicePermissionsPrompt::Prompt::GetPromptMessage() const {
  return l10n_util::GetStringFUTF16(multiple_
                                        ? IDS_DEVICE_PERMISSIONS_PROMPT_MULTIPLE
                                        : IDS_DEVICE_PERMISSIONS_PROMPT_SINGLE,
                                    base::UTF8ToUTF16(extension_->name()));
}

scoped_refptr<UsbDevice> DevicePermissionsPrompt::Prompt::GetDevice(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index].device;
}

void DevicePermissionsPrompt::Prompt::GrantDevicePermission(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(browser_context_);
  if (permissions_manager) {
    const DeviceInfo& device = devices_[index];
    permissions_manager->AllowUsbDevice(
        extension_->id(), device.device, device.original_product_string,
        device.original_manufacturer_string, device.serial_number);
  }
}

void DevicePermissionsPrompt::Prompt::set_filters(
    const std::vector<UsbDeviceFilter>& filters) {
  filters_ = filters;
}

DevicePermissionsPrompt::Prompt::~Prompt() {
}

void DevicePermissionsPrompt::Prompt::DoDeviceQuery() {
  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    return;
  }

  std::vector<scoped_refptr<UsbDevice>> devices;
  service->GetDevices(&devices);

  std::vector<DeviceInfo> device_info;
  for (const auto& device : devices) {
    if (!(filters_.empty() || UsbDeviceFilter::MatchesAny(device, filters_))) {
      continue;
    }

    device_info.push_back(DeviceInfo(device));
  }

  if (!usb_service_observer_.IsObserving(service)) {
    usb_service_observer_.Add(service);
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevicePermissionsPrompt::Prompt::SetDevices, this, device_info));
}

void DevicePermissionsPrompt::Prompt::SetDevices(
    const std::vector<DeviceInfo>& devices) {
  devices_ = devices;
  if (observer_) {
    observer_->OnDevicesChanged();
  }
}

void DevicePermissionsPrompt::Prompt::AddDevice(const DeviceInfo& device) {
  devices_.push_back(device);
  if (observer_) {
    observer_->OnDevicesChanged();
  }
}

void DevicePermissionsPrompt::Prompt::RemoveDevice(
    scoped_refptr<UsbDevice> device) {
  bool removed_entry = false;
  for (std::vector<DeviceInfo>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (it->device == device) {
      devices_.erase(it);
      removed_entry = true;
      break;
    }
  }
  if (observer_ && removed_entry) {
    observer_->OnDevicesChanged();
  }
}

void DevicePermissionsPrompt::Prompt::OnDeviceAdded(
    scoped_refptr<UsbDevice> device) {
  if (!(filters_.empty() || UsbDeviceFilter::MatchesAny(device, filters_))) {
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DevicePermissionsPrompt::Prompt::AddDevice, this,
                 DeviceInfo(device)));
}

void DevicePermissionsPrompt::Prompt::OnDeviceRemoved(
    scoped_refptr<UsbDevice> device) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DevicePermissionsPrompt::Prompt::RemoveDevice, this, device));
}

DevicePermissionsPrompt::DevicePermissionsPrompt(
    content::WebContents* web_contents)
    : web_contents_(web_contents), delegate_(nullptr) {
}

DevicePermissionsPrompt::~DevicePermissionsPrompt() {
}

void DevicePermissionsPrompt::AskForUsbDevices(
    Delegate* delegate,
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    const std::vector<UsbDeviceFilter>& filters) {
  prompt_ = new Prompt();
  prompt_->set_extension(extension);
  prompt_->set_browser_context(context);
  prompt_->set_multiple(multiple);
  prompt_->set_filters(filters);
  delegate_ = delegate;

  ShowDialog();
}

}  // namespace extensions
