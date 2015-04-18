// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DEVICE_PERMISSIONS_PROMPT_H_
#define EXTENSIONS_BROWSER_DEVICE_PERMISSIONS_PROMPT_H_

#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "content/public/browser/browser_thread.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace device {
class UsbDeviceFilter;
}

namespace extensions {

class Extension;

// Platform-independent interface for displaing a UI for choosing devices
// (similar to choosing files).
class DevicePermissionsPrompt {
 public:
  class Delegate {
   public:
    // Called with the list of selected USB devices.
    virtual void OnUsbDevicesChosen(
        const std::vector<scoped_refptr<device::UsbDevice>>& devices) = 0;

   protected:
    virtual ~Delegate();
  };

  // Context information available to the UI implementation.
  class Prompt : public base::RefCounted<Prompt>,
                 public device::UsbService::Observer {
   public:
    // Displayed properties of a device.
    struct DeviceInfo {
      DeviceInfo(scoped_refptr<device::UsbDevice> device);
      ~DeviceInfo();

      scoped_refptr<device::UsbDevice> device;
      base::string16 name;
      bool granted = false;
    };

    // Since the set of devices can change while the UI is visible an
    // implementation should register an observer.
    class Observer {
     public:
      virtual void OnDevicesChanged() = 0;

     protected:
      virtual ~Observer();
    };

    Prompt(Delegate* delegate,
           const Extension* extension,
           content::BrowserContext* context);

    // Only one observer may be registered at a time.
    void SetObserver(Observer* observer);

    base::string16 GetHeading() const;
    base::string16 GetPromptMessage() const;
    size_t GetDeviceCount() const { return devices_.size(); }
    base::string16 GetDeviceName(size_t index) const;
    base::string16 GetDeviceSerialNumber(size_t index) const;

    // Notifies the DevicePermissionsManager for the current extension that
    // access to the device at the given index is now granted.
    void GrantDevicePermission(size_t index);
    void Dismissed();

    bool multiple() const { return multiple_; }

    void set_multiple(bool multiple) { multiple_ = multiple; }
    void set_filters(const std::vector<device::UsbDeviceFilter>& filters);

   private:
    friend class base::RefCounted<Prompt>;

    virtual ~Prompt();

    // device::UsbService::Observer implementation:
    void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;
    void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

    void OnDevicesEnumerated(
        const std::vector<scoped_refptr<device::UsbDevice>>& devices);
    void AddCheckedUsbDevice(scoped_refptr<device::UsbDevice> device,
                             bool allowed);

    const extensions::Extension* extension_ = nullptr;
    content::BrowserContext* browser_context_ = nullptr;
    Delegate* delegate_ = nullptr;
    bool multiple_ = false;
    std::vector<device::UsbDeviceFilter> filters_;
    std::vector<DeviceInfo> devices_;
    Observer* observer_ = nullptr;
    ScopedObserver<device::UsbService, device::UsbService::Observer>
        usb_service_observer_;
  };

  DevicePermissionsPrompt(content::WebContents* web_contents);
  virtual ~DevicePermissionsPrompt();

  void AskForUsbDevices(Delegate* delegate,
                        const Extension* extension,
                        content::BrowserContext* context,
                        bool multiple,
                        const std::vector<device::UsbDeviceFilter>& filters);

 protected:
  virtual void ShowDialog() = 0;

  content::WebContents* web_contents() { return web_contents_; }
  scoped_refptr<Prompt> prompt() { return prompt_; }

 private:
  // Parent web contents of the device permissions UI dialog.
  content::WebContents* web_contents_;

  // Parameters available to the UI implementation.
  scoped_refptr<Prompt> prompt_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_PROMPT_H_
