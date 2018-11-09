// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "device/fido/ble/fido_ble_discovery.h"
#include "device/fido/buildflags.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_discovery_base.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/hid/fido_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN) && BUILDFLAG(USE_WIN_WEBAUTHN_API)
#include <Winuser.h>
#include "device/fido/win/discovery.h"
#include "device/fido/win/webauthn_api.h"
#endif

namespace device {

namespace {

std::unique_ptr<FidoDiscoveryBase> CreateUsbFidoDiscovery(
    service_manager::Connector* connector) {
#if defined(OS_ANDROID)
  NOTREACHED() << "USB HID not supported on Android.";
  return nullptr;
#else

#if defined(OS_WIN) && BUILDFLAG(USE_WIN_WEBAUTHN_API)
  // On platforms where the Windows webauthn.dll is present, access to USB
  // devices is blocked and we use a special authenticator that forwards
  // requests to the Windows WebAuthn API instead.
  if (base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) &&
      WinWebAuthnApi::GetDefault()->IsAvailable()) {
    return std::make_unique<WinNativeCrossPlatformAuthenticatorDiscovery>(
        WinWebAuthnApi::GetDefault(),
        // TODO(martinkr): Inject the window from which the request originated.
        GetForegroundWindow());
  }
#endif  // defined(OS_WIN) && BUILDFLAG(USE_WIN_WEBAUTHN_API)

  DCHECK(connector);
  return std::make_unique<FidoHidDiscovery>(connector);
#endif  // !defined(OS_ANDROID)
}

std::unique_ptr<FidoDiscoveryBase> CreateFidoDiscoveryImpl(
    FidoTransportProtocol transport,
    service_manager::Connector* connector) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return CreateUsbFidoDiscovery(connector);
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return std::make_unique<FidoBleDiscovery>();
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      NOTREACHED() << "Cable discovery is constructed using the dedicated "
                      "factory method.";
      return nullptr;
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(https://crbug.com/825949): Add NFC support.
      return nullptr;
    case FidoTransportProtocol::kInternal:
      NOTREACHED() << "Internal authenticators should be handled separately.";
      return nullptr;
  }
  NOTREACHED() << "Unhandled transport type";
  return nullptr;
}

}  // namespace

std::unique_ptr<FidoDiscoveryBase> CreateCableDiscoveryImpl(
    std::vector<CableDiscoveryData> cable_data) {
  return std::make_unique<FidoCableDiscovery>(std::move(cable_data));
}

// static
FidoDiscoveryFactory::FactoryFuncPtr FidoDiscoveryFactory::g_factory_func_ =
    &CreateFidoDiscoveryImpl;

// static
FidoDiscoveryFactory::CableFactoryFuncPtr
    FidoDiscoveryFactory::g_cable_factory_func_ = &CreateCableDiscoveryImpl;

// static
std::unique_ptr<FidoDiscoveryBase> FidoDiscoveryFactory::Create(
    FidoTransportProtocol transport,
    service_manager::Connector* connector) {
  return (*g_factory_func_)(transport, connector);
}

//  static
std::unique_ptr<FidoDiscoveryBase> FidoDiscoveryFactory::CreateCable(
    std::vector<CableDiscoveryData> cable_data) {
  return (*g_cable_factory_func_)(std::move(cable_data));
}

// ScopedFidoDiscoveryFactory -------------------------------------------------

namespace internal {

ScopedFidoDiscoveryFactory::ScopedFidoDiscoveryFactory() {
  DCHECK(!g_current_factory);
  g_current_factory = this;
  original_factory_func_ =
      std::exchange(FidoDiscoveryFactory::g_factory_func_,
                    &ForwardCreateFidoDiscoveryToCurrentFactory);
  original_cable_factory_func_ =
      std::exchange(FidoDiscoveryFactory::g_cable_factory_func_,
                    &ForwardCreateCableDiscoveryToCurrentFactory);
}

ScopedFidoDiscoveryFactory::~ScopedFidoDiscoveryFactory() {
  g_current_factory = nullptr;
  FidoDiscoveryFactory::g_factory_func_ = original_factory_func_;
  FidoDiscoveryFactory::g_cable_factory_func_ = original_cable_factory_func_;
}

// static
std::unique_ptr<FidoDiscoveryBase>
ScopedFidoDiscoveryFactory::ForwardCreateFidoDiscoveryToCurrentFactory(
    FidoTransportProtocol transport,
    ::service_manager::Connector* connector) {
  DCHECK(g_current_factory);
  return g_current_factory->CreateFidoDiscovery(transport, connector);
}

// static
std::unique_ptr<FidoDiscoveryBase>
ScopedFidoDiscoveryFactory::ForwardCreateCableDiscoveryToCurrentFactory(
    std::vector<CableDiscoveryData> cable_data) {
  DCHECK(g_current_factory);
  g_current_factory->set_last_cable_data(std::move(cable_data));
  return g_current_factory->CreateFidoDiscovery(
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
      nullptr /* connector */);
}

// static
ScopedFidoDiscoveryFactory* ScopedFidoDiscoveryFactory::g_current_factory =
    nullptr;

}  // namespace internal
}  // namespace device
