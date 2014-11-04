// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_VPN_PROVIDER_VPN_PROVIDER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_VPN_PROVIDER_VPN_PROVIDER_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class VpnProviderCreateConfigFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.createConfig",
                             VPNPROVIDER_CREATECONFIG);

 protected:
  virtual ~VpnProviderCreateConfigFunction();

  virtual ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderDestroyConfigFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.destroyConfig",
                             VPNPROVIDER_DESTROYCONFIG);

 protected:
  virtual ~VpnProviderDestroyConfigFunction();

  virtual ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderSetParametersFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.setParameters",
                             VPNPROVIDER_SETPARAMETERS);

 protected:
  virtual ~VpnProviderSetParametersFunction();

  virtual ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderSendPacketFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.sendPacket", VPNPROVIDER_SENDPACKET);

 protected:
  virtual ~VpnProviderSendPacketFunction();

  virtual ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderNotifyConnectionStateChangedFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.notifyConnectionStateChanged",
                             VPNPROVIDER_NOTIFYCONNECTIONSTATECHANGED);

 protected:
  virtual ~VpnProviderNotifyConnectionStateChangedFunction();

  virtual ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_VPN_PROVIDER_VPN_PROVIDER_API_H_
