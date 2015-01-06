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
#include "device/usb/usb_service.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace device {
class UsbDevice;
class UsbDeviceFilter;
}

namespace extensions {

class Extension;

// Platform-independent interface for displaing a UI for choosing devices
// (similar to choosing files).
class DevicePermissionsPrompt {
 public:
  // Context information available to the UI implementation.
  class Prompt : public base::RefCountedThreadSafe<
                     Prompt,
                     content::BrowserThread::DeleteOnFileThread>,
                 public device::UsbService::Observer {
   public:
    // Displayed properties of a device.
    struct DeviceInfo {
      DeviceInfo(scoped_refptr<device::UsbDevice> device);
      ~DeviceInfo();

      scoped_refptr<device::UsbDevice> device;
      base::string16 name;
      base::string16 original_manufacturer_string;
      base::string16 original_product_string;
      base::string16 serial_number;
    };

    // Since the set of devices can change while the UI is visible an
    // implementation should register an observer.
    class Observer {
     public:
      virtual void OnDevicesChanged() = 0;
    };

    Prompt();

    // Only one observer may be registered at a time.
    void SetObserver(Observer* observer);

    base::string16 GetHeading() const;
    base::string16 GetPromptMessage() const;
    size_t GetDeviceCount() const { return devices_.size(); }
    scoped_refptr<device::UsbDevice> GetDevice(size_t index) const;
    base::string16 GetDeviceName(size_t index) const {
      DCHECK_LT(index, devices_.size());
      return devices_[index].name;
    }
    base::string16 GetDeviceSerialNumber(size_t index) const {
      DCHECK_LT(index, devices_.size());
      return devices_[index].serial_number;
    }

    // Notifies the DevicePermissionsManager for the current extension that
    // access to the device at the given index is now granted.
    void GrantDevicePermission(size_t index) const;

    const extensions::Extension* extension() const { return extension_; }
    void set_extension(const extensions::Extension* extension) {
      extension_ = extension;
    }

    void set_browser_context(content::BrowserContext* context) {
      browser_context_ = context;
    }

    bool multiple() const { return multiple_; }
    void set_multiple(bool multiple) { multiple_ = multiple; }

    const std::vector<device::UsbDeviceFilter>& filters() const {
      return filters_;
    }
    void set_filters(const std::vector<device::UsbDeviceFilter>& filters);

   private:
    friend struct content::BrowserThread::DeleteOnThread<
        content::BrowserThread::FILE>;
    friend class base::DeleteHelper<Prompt>;

    virtual ~Prompt();

    // Querying for devices must be done asynchronously on the FILE thread.
    void DoDeviceQuery();
    void SetDevices(const std::vector<DeviceInfo>& devices);
    void AddDevice(const DeviceInfo& device);
    void RemoveDevice(scoped_refptr<device::UsbDevice> device);

    // device::UsbService::Observer implementation:
    void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;
    void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

    const extensions::Extension* extension_;
    content::BrowserContext* browser_context_;
    bool multiple_;
    std::vector<device::UsbDeviceFilter> filters_;
    std::vector<DeviceInfo> devices_;
    Observer* observer_;
    ScopedObserver<device::UsbService, device::UsbService::Observer>
        usb_service_observer_;
  };

  class Delegate {
   public:
    // Called with the list of selected USB devices.
    virtual void OnUsbDevicesChosen(
        const std::vector<scoped_refptr<device::UsbDevice>>& devices) = 0;

   protected:
    virtual ~Delegate() {}
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
  Delegate* delegate() { return delegate_; }
  scoped_refptr<Prompt> prompt() { return prompt_; }

 private:
  // Parent web contents of the device permissions UI dialog.
  content::WebContents* web_contents_;

  // The delegate called after the UI has been dismissed.
  Delegate* delegate_;

  // Parameters available to the UI implementation.
  scoped_refptr<Prompt> prompt_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_PROMPT_H_
