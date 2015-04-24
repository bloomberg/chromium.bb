// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_prompt.h"

#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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

namespace {

class UsbDeviceInfo : public DevicePermissionsPrompt::Prompt::DeviceInfo {
 public:
  UsbDeviceInfo(scoped_refptr<UsbDevice> device) : device_(device) {
    base::string16 manufacturer_string = device->manufacturer_string();
    if (manufacturer_string.empty()) {
      const char* vendor_name =
          device::UsbIds::GetVendorName(device->vendor_id());
      if (vendor_name) {
        manufacturer_string = base::UTF8ToUTF16(vendor_name);
      } else {
        base::string16 vendor_id = base::ASCIIToUTF16(
            base::StringPrintf("0x%04x", device->vendor_id()));
        manufacturer_string =
            l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_VENDOR, vendor_id);
      }
    }

    base::string16 product_string = device->product_string();
    if (product_string.empty()) {
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

    name_ = l10n_util::GetStringFUTF16(IDS_DEVICE_PERMISSIONS_DEVICE_NAME,
                                       product_string, manufacturer_string);
    serial_number_ = device->serial_number();
  }

  ~UsbDeviceInfo() override {}

  const scoped_refptr<UsbDevice>& device() const { return device_; }

 private:
  // TODO(reillyg): Convert this to a weak reference when UsbDevice has a
  // connected flag.
  scoped_refptr<UsbDevice> device_;
};

class UsbDevicePermissionsPrompt : public DevicePermissionsPrompt::Prompt,
                                   public device::UsbService::Observer {
 public:
  UsbDevicePermissionsPrompt(
      const Extension* extension,
      content::BrowserContext* context,
      bool multiple,
      const std::vector<device::UsbDeviceFilter>& filters,
      const DevicePermissionsPrompt::UsbDevicesCallback& callback)
      : Prompt(extension, context, multiple),
        filters_(filters),
        callback_(callback),
        service_observer_(this) {}

 private:
  ~UsbDevicePermissionsPrompt() override {}

  // DevicePermissionsPrompt::Prompt implementation:
  void SetObserver(
      DevicePermissionsPrompt::Prompt::Observer* observer) override {
    DevicePermissionsPrompt::Prompt::SetObserver(observer);

    if (observer) {
      UsbService* service = device::DeviceClient::Get()->GetUsbService();
      if (service && !service_observer_.IsObserving(service)) {
        service->GetDevices(
            base::Bind(&UsbDevicePermissionsPrompt::OnDevicesEnumerated, this));
        service_observer_.Add(service);
      }
    }
  }

  base::string16 GetHeading() const override {
    return l10n_util::GetStringUTF16(
        multiple() ? IDS_USB_DEVICE_PERMISSIONS_PROMPT_TITLE_MULTIPLE
                   : IDS_USB_DEVICE_PERMISSIONS_PROMPT_TITLE_SINGLE);
  }

  void Dismissed() override {
    DevicePermissionsManager* permissions_manager =
        DevicePermissionsManager::Get(browser_context());
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (const DeviceInfo* device : devices_) {
      if (device->granted()) {
        const UsbDeviceInfo* usb_device =
            static_cast<const UsbDeviceInfo*>(device);
        devices.push_back(usb_device->device());
        if (permissions_manager) {
          permissions_manager->AllowUsbDevice(extension()->id(),
                                              usb_device->device());
        }
      }
    }
    DCHECK(multiple() || devices.size() <= 1);
    callback_.Run(devices);
    callback_.Reset();
  }

  // device::UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override {
    if (!(filters_.empty() || UsbDeviceFilter::MatchesAny(device, filters_))) {
      return;
    }

    device->CheckUsbAccess(base::Bind(
        &UsbDevicePermissionsPrompt::AddCheckedDevice, this, device));
  }

  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      const UsbDeviceInfo* entry = static_cast<const UsbDeviceInfo*>(*it);
      if (entry->device() == device) {
        devices_.erase(it);
        if (observer()) {
          observer()->OnDevicesChanged();
        }
        return;
      }
    }
  }

  void OnDevicesEnumerated(
      const std::vector<scoped_refptr<UsbDevice>>& devices) {
    for (const auto& device : devices) {
      OnDeviceAdded(device);
    }
  }

  void AddCheckedDevice(scoped_refptr<UsbDevice> device, bool allowed) {
    if (allowed) {
      // TODO(reillyg): This method could be called after OnDeviceRemoved. We
      // should check that the device is still connected.
      devices_.push_back(new UsbDeviceInfo(device));
      if (observer()) {
        observer()->OnDevicesChanged();
      }
    }
  }

  std::vector<UsbDeviceFilter> filters_;
  DevicePermissionsPrompt::UsbDevicesCallback callback_;
  ScopedObserver<UsbService, UsbService::Observer> service_observer_;
};

}  // namespace

DevicePermissionsPrompt::Prompt::DeviceInfo::DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::DeviceInfo::~DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::Observer::~Observer() {
}

DevicePermissionsPrompt::Prompt::Prompt(const Extension* extension,
                                        content::BrowserContext* context,
                                        bool multiple)
    : extension_(extension), browser_context_(context), multiple_(multiple) {
}

void DevicePermissionsPrompt::Prompt::SetObserver(Observer* observer) {
  observer_ = observer;
}

base::string16 DevicePermissionsPrompt::Prompt::GetPromptMessage() const {
  return l10n_util::GetStringFUTF16(multiple_
                                        ? IDS_DEVICE_PERMISSIONS_PROMPT_MULTIPLE
                                        : IDS_DEVICE_PERMISSIONS_PROMPT_SINGLE,
                                    base::UTF8ToUTF16(extension_->name()));
}

base::string16 DevicePermissionsPrompt::Prompt::GetDeviceName(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index]->name();
}

base::string16 DevicePermissionsPrompt::Prompt::GetDeviceSerialNumber(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index]->serial_number();
}

void DevicePermissionsPrompt::Prompt::GrantDevicePermission(size_t index) {
  DCHECK_LT(index, devices_.size());
  devices_[index]->set_granted();
}

DevicePermissionsPrompt::Prompt::~Prompt() {
}

DevicePermissionsPrompt::DevicePermissionsPrompt(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

DevicePermissionsPrompt::~DevicePermissionsPrompt() {
}

void DevicePermissionsPrompt::AskForUsbDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    const std::vector<UsbDeviceFilter>& filters,
    const UsbDevicesCallback& callback) {
  prompt_ = new UsbDevicePermissionsPrompt(extension, context, multiple,
                                           filters, callback);
  ShowDialog();
}

}  // namespace extensions
