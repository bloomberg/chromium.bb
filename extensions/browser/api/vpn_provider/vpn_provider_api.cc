// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/vpn_provider/vpn_provider_api.h"

namespace extensions {

VpnProviderCreateConfigFunction::~VpnProviderCreateConfigFunction() {
}

ExtensionFunction::ResponseAction VpnProviderCreateConfigFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(-1)));
}

VpnProviderDestroyConfigFunction::~VpnProviderDestroyConfigFunction() {
}

ExtensionFunction::ResponseAction VpnProviderDestroyConfigFunction::Run() {
  return RespondNow(NoArguments());
}

VpnProviderSetParametersFunction::~VpnProviderSetParametersFunction() {
}

ExtensionFunction::ResponseAction VpnProviderSetParametersFunction::Run() {
  return RespondNow(NoArguments());
}

VpnProviderSendPacketFunction::~VpnProviderSendPacketFunction() {
}

ExtensionFunction::ResponseAction VpnProviderSendPacketFunction::Run() {
  return RespondNow(NoArguments());
}

VpnProviderNotifyConnectionStateChangedFunction::
    ~VpnProviderNotifyConnectionStateChangedFunction() {
}

ExtensionFunction::ResponseAction
VpnProviderNotifyConnectionStateChangedFunction::Run() {
  return RespondNow(NoArguments());
}

}  // namespace extensions
