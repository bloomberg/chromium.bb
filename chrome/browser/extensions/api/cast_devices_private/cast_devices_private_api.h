// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CAST_DEVICES_PRIVATE_CAST_DEVICES_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CAST_DEVICES_PRIVATE_CAST_DEVICES_PRIVATE_API_H_

#include <vector>

#include "ash/cast_config_delegate.h"
#include "base/callback_list.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class CastDeviceUpdateListeners : public BrowserContextKeyedAPI {
 public:
  explicit CastDeviceUpdateListeners(content::BrowserContext* context);
  ~CastDeviceUpdateListeners() override;

  // Fetch an instance for the given context.
  static CastDeviceUpdateListeners* Get(content::BrowserContext* context);

  // Register a function that will be invoked only when a new device update is
  // available.
  ash::CastConfigDelegate::DeviceUpdateSubscription RegisterCallback(
      const ash::CastConfigDelegate::ReceiversAndActivitesCallback& callback);

  // BrowserContextKeyedAPI implementation:
  static BrowserContextKeyedAPIFactory<CastDeviceUpdateListeners>*
  GetFactoryInstance();
  static const bool kServiceIsCreatedWithBrowserContext = false;

 private:
  using ReceiverAndActivityList =
    std::vector<ash::CastConfigDelegate::ReceiverAndActivity>;

  friend class CastDevicesPrivateUpdateDevicesFunction;  // For NotifyCallbacks.
  void NotifyCallbacks(const ReceiverAndActivityList& devices);

  base::CallbackList<void(const ReceiverAndActivityList&)> callback_list_;

  friend class BrowserContextKeyedAPIFactory<CastDeviceUpdateListeners>;

  // BrowserContextKeyedAPI implementation:
  static const char* service_name() { return "CastDeviceUpdateListeners"; }

  DISALLOW_COPY_AND_ASSIGN(CastDeviceUpdateListeners);
};

// static void updateDeviceState(ReceiverActivity[] devices);
class CastDevicesPrivateUpdateDevicesFunction :
    public UIThreadExtensionFunction {
 public:
  CastDevicesPrivateUpdateDevicesFunction();

 private:
  ~CastDevicesPrivateUpdateDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("cast.devicesPrivate.updateDevices",
                             CASTDEVICESPRIVATE_UPDATEDEVICES);
  DISALLOW_COPY_AND_ASSIGN(CastDevicesPrivateUpdateDevicesFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CAST_DEVICES_PRIVATE_CAST_DEVICES_PRIVATE_API_H_
