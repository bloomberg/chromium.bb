// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_device_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/vr/service/browser_xr_device.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_manager_connection.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/vr_device_provider.h"

#if defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_ARCORE)
#include "device/vr/android/arcore/arcore_device_provider_factory.h"
#endif

#include "device/vr/android/gvr/gvr_device_provider.h"
#endif

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device_provider.h"
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
#include "device/vr/oculus/oculus_device_provider.h"
#endif

namespace vr {

namespace {
VRDeviceManager* g_vr_device_manager = nullptr;
}  // namespace

VRDeviceManager* VRDeviceManager::GetInstance() {
  if (!g_vr_device_manager) {
    // Register VRDeviceProviders for the current platform
    ProviderList providers;

#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_ARCORE)
    if (base::FeatureList::IsEnabled(features::kWebXrHitTest)) {
      providers.emplace_back(device::ARCoreDeviceProviderFactory::Create());
    }
#endif

    providers.emplace_back(std::make_unique<device::GvrDeviceProvider>());
#endif

#if BUILDFLAG(ENABLE_OPENVR)
    if (base::FeatureList::IsEnabled(features::kOpenVR))
      providers.emplace_back(std::make_unique<device::OpenVRDeviceProvider>());
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
    // For now, only use the Oculus when OpenVR is not enabled.
    // TODO(billorr): Add more complicated logic to avoid routing Oculus devices
    // through OpenVR.
    if (base::FeatureList::IsEnabled(features::kOculusVR) &&
        providers.size() == 0)
      providers.emplace_back(
          std::make_unique<device::OculusVRDeviceProvider>());
#endif

    if (base::FeatureList::IsEnabled(features::kWebXrOrientationSensorDevice)) {
      content::ServiceManagerConnection* connection =
          content::ServiceManagerConnection::GetForProcess();
      if (connection) {
        providers.emplace_back(
            std::make_unique<device::VROrientationDeviceProvider>(
                connection->GetConnector()));
      }
    }

    // The constructor sets g_vr_device_manager, which is cleaned up when
    // RemoveService is called, when the last active VRServiceImpl is destroyed.
    new VRDeviceManager(std::move(providers));
  }
  return g_vr_device_manager;
}

BrowserXrDevice* VRDeviceManager::GetImmersiveDevice() {
#if defined(OS_ANDROID)
  auto* gvr = GetDevice(device::VRDeviceId::GVR_DEVICE_ID);
  if (gvr)
    return gvr;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  auto* openvr = GetDevice(device::VRDeviceId::OPENVR_DEVICE_ID);
  if (openvr)
    return openvr;
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  auto* oculus = GetDevice(device::VRDeviceId::OCULUS_DEVICE_ID);
  if (oculus)
    return oculus;
#endif

  return nullptr;
}

BrowserXrDevice* VRDeviceManager::GetDevice(device::VRDeviceId id) {
  auto it = devices_.find(static_cast<unsigned int>(id));
  if (it == devices_.end())
    return nullptr;

  return it->second.get();
}

BrowserXrDevice* VRDeviceManager::GetDeviceForOptions(
    device::mojom::XRSessionOptions* options) {
  // Examine options to determine which device provider we should use.
  if (options->immersive && !options->provide_passthrough_camera) {
    return GetImmersiveDevice();
  } else if (options->provide_passthrough_camera && !options->immersive) {
    return GetDevice(device::VRDeviceId::ARCORE_DEVICE_ID);
  } else if (!options->provide_passthrough_camera && !options->immersive) {
    // Magic window session.
    // Try the orientation provider if it exists.
    auto* orientation_device =
        GetDevice(device::VRDeviceId::ORIENTATION_DEVICE_ID);
    if (orientation_device) {
      return orientation_device;
    }

    // Otherwise fall back to immersive providers.
    return GetImmersiveDevice();
  }
  return nullptr;
}

bool VRDeviceManager::HasInstance() {
  return g_vr_device_manager != nullptr;
}

VRDeviceManager::VRDeviceManager(ProviderList providers)
    : providers_(std::move(providers)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!g_vr_device_manager);
  g_vr_device_manager = this;
}

VRDeviceManager::~VRDeviceManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_vr_device_manager = nullptr;
}

void VRDeviceManager::AddService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Loop through any currently active devices and send Connected messages to
  // the service. Future devices that come online will send a Connected message
  // when they are created.
  InitializeProviders();

  if (AreAllProvidersInitialized())
    service->InitializationComplete();

  services_.insert(service);
}

void VRDeviceManager::RemoveService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  services_.erase(service);

  if (services_.empty()) {
    // Delete the device manager when it has no active connections.
    delete g_vr_device_manager;
  }
}

void VRDeviceManager::AddDevice(unsigned int id,
                                device::mojom::VRDisplayInfoPtr info,
                                device::mojom::XRRuntimePtr runtime) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(devices_.find(id) == devices_.end());

  devices_[id] =
      std::make_unique<BrowserXrDevice>(std::move(runtime), std::move(info));
  for (VRServiceImpl* service : services_)
    service->ConnectDevice(devices_[id].get());
}

void VRDeviceManager::RemoveDevice(unsigned int id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = devices_.find(id);
  DCHECK(it != devices_.end());

  // Remove the device from devices_ before notifying services that it was
  // removed, since they will query for devices in RemoveDevice.
  std::unique_ptr<BrowserXrDevice> removed_device = std::move(it->second);
  devices_.erase(it);

  for (VRServiceImpl* service : services_)
    service->RemoveDevice(removed_device.get());
}

void VRDeviceManager::RecordVrStartupHistograms() {
#if BUILDFLAG(ENABLE_OPENVR)
  device::OpenVRDeviceProvider::RecordRuntimeAvailability();
#endif
}

device::mojom::XRRuntime* VRDeviceManager::GetRuntime(unsigned int id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (id == 0)
    return nullptr;

  DeviceMap::iterator iter = devices_.find(id);
  if (iter == devices_.end())
    return nullptr;

  return iter->second->GetRuntime();
}

void VRDeviceManager::InitializeProviders() {
  if (providers_initialized_)
    return;

  for (const auto& provider : providers_) {
    provider->Initialize(base::BindRepeating(&VRDeviceManager::AddDevice,
                                             base::Unretained(this)),
                         base::BindRepeating(&VRDeviceManager::RemoveDevice,
                                             base::Unretained(this)),
                         base::BindOnce(&VRDeviceManager::OnProviderInitialized,
                                        base::Unretained(this)));
  }

  providers_initialized_ = true;
}

void VRDeviceManager::OnProviderInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++num_initialized_providers_;
  if (AreAllProvidersInitialized()) {
    for (VRServiceImpl* service : services_)
      service->InitializationComplete();
  }
}

bool VRDeviceManager::AreAllProvidersInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return num_initialized_providers_ == providers_.size();
}

size_t VRDeviceManager::NumberOfConnectedServices() {
  return services_.size();
}

}  // namespace vr
