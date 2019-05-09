// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/usb/usb_policy_allowed_devices.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/public/mojom/device_manager_client.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

class UsbChooserContext : public ChooserContextBase,
                          public device::mojom::UsbDeviceManagerClient {
 public:
  explicit UsbChooserContext(Profile* profile);
  ~UsbChooserContext() override;

  // This observer can be used to be notified of changes to USB devices that are
  // connected.
  class DeviceObserver : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::UsbDeviceInfo&);
    virtual void OnDeviceRemoved(const device::mojom::UsbDeviceInfo&);
    virtual void OnDeviceManagerConnectionError();
  };

  static std::unique_ptr<base::DictionaryValue> DeviceInfoToDictValue(
      const device::mojom::UsbDeviceInfo& device_info);

  // These methods from ChooserContextBase are overridden in order to expose
  // ephemeral devices through the public interface.
  std::vector<std::unique_ptr<ChooserContextBase::Object>> GetGrantedObjects(
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  std::vector<std::unique_ptr<ChooserContextBase::Object>>
  GetAllGrantedObjects() override;
  void RevokeObjectPermission(const GURL& requesting_origin,
                              const GURL& embedding_origin,
                              const base::DictionaryValue& object) override;

  // Grants |requesting_origin| access to the USB device.
  void GrantDevicePermission(const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             const device::mojom::UsbDeviceInfo& device_info);

  // Checks if |requesting_origin| (when embedded within |embedding_origin| has
  // access to a device with |device_info|.
  bool HasDevicePermission(const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           const device::mojom::UsbDeviceInfo& device_info);

  void AddObserver(DeviceObserver* observer);
  void RemoveObserver(DeviceObserver* observer);

  // Forward UsbDeviceManager methods.
  void GetDevices(device::mojom::UsbDeviceManager::GetDevicesCallback callback);
  void GetDevice(const std::string& guid,
                 device::mojom::UsbDeviceRequest device_request,
                 device::mojom::UsbDeviceClientPtr device_client);
#if defined(OS_ANDROID)
  void RefreshDeviceInfo(
      const std::string& guid,
      device::mojom::UsbDeviceManager::RefreshDeviceInfoCallback callback);
#endif

  // This method should only be called when you are sure that |devices_| has
  // been initialized. It will return nullptr if the guid cannot be found.
  const device::mojom::UsbDeviceInfo* GetDeviceInfo(const std::string& guid);

  base::WeakPtr<UsbChooserContext> AsWeakPtr();

  void SetDeviceManagerForTesting(
      device::mojom::UsbDeviceManagerPtr fake_device_manager);

  // ChooserContextBase implementation.
  bool IsValidObject(const base::DictionaryValue& object) override;
  std::string GetObjectName(const base::DictionaryValue& object) override;
  void InitDeviceList(std::vector<::device::mojom::UsbDeviceInfoPtr> devices);

 private:
  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  void OnDeviceManagerConnectionError();
  void EnsureConnectionWithDeviceManager();
  void SetUpDeviceManagerConnection();
#if defined(OS_ANDROID)
  void OnDeviceInfoRefreshed(
      device::mojom::UsbDeviceManager::RefreshDeviceInfoCallback callback,
      device::mojom::UsbDeviceInfoPtr device_info);
#endif

  bool is_incognito_;
  bool is_initialized_ = false;
  base::queue<device::mojom::UsbDeviceManager::GetDevicesCallback>
      pending_get_devices_requests_;

  std::map<std::pair<GURL, GURL>, std::set<std::string>> ephemeral_devices_;
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> devices_;

  std::unique_ptr<UsbPolicyAllowedDevices> usb_policy_allowed_devices_;

  // Connection to |device_manager_instance_|.
  device::mojom::UsbDeviceManagerPtr device_manager_;
  mojo::AssociatedBinding<device::mojom::UsbDeviceManagerClient>
      client_binding_;
  base::ObserverList<DeviceObserver> device_observer_list_;

  base::WeakPtrFactory<UsbChooserContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbChooserContext);
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_H_
