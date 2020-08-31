// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/public/browser/xr_runtime_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "content/browser/xr/xr_utils.h"
#include "content/public/browser/device_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

#if !defined(OS_ANDROID)
#include "content/browser/xr/service/isolated_device_provider.h"
#endif  // !defined(OS_ANDROID)

namespace content {

namespace {
XRRuntimeManagerImpl* g_xr_runtime_manager = nullptr;

base::LazyInstance<base::ObserverList<XRRuntimeManager::Observer>>::Leaky
    g_xr_runtime_manager_observers;

#if !defined(OS_ANDROID)
bool IsEnabled(const base::CommandLine* command_line,
               const base::Feature& feature,
               const std::string& name) {
  if (!command_line->HasSwitch(switches::kWebXrForceRuntime))
    return base::FeatureList::IsEnabled(feature);

  return (base::CompareCaseInsensitiveASCII(
              command_line->GetSwitchValueASCII(switches::kWebXrForceRuntime),
              name) == 0);
}
#endif

}  // namespace

// XRRuntimeManager statics
XRRuntimeManager* XRRuntimeManager::GetInstanceIfCreated() {
  return g_xr_runtime_manager;
}

void XRRuntimeManager::AddObserver(XRRuntimeManager::Observer* observer) {
  g_xr_runtime_manager_observers.Get().AddObserver(observer);
}

void XRRuntimeManager::RemoveObserver(XRRuntimeManager::Observer* observer) {
  g_xr_runtime_manager_observers.Get().RemoveObserver(observer);
}

void XRRuntimeManager::ExitImmersivePresentation() {
  if (!g_xr_runtime_manager) {
    return;
  }

  auto* browser_xr_runtime =
      g_xr_runtime_manager->GetCurrentlyPresentingImmersiveRuntime();
  if (!browser_xr_runtime) {
    return;
  }

  browser_xr_runtime->ExitActiveImmersiveSession();
}

// Static
scoped_refptr<XRRuntimeManagerImpl>
XRRuntimeManagerImpl::GetOrCreateInstance() {
  if (g_xr_runtime_manager) {
    return base::WrapRefCounted(g_xr_runtime_manager);
  }

  // Start by getting any providers specified by the XrIntegrationClient
  XRProviderList providers;
  auto* integration_client = GetXrIntegrationClient();

  if (integration_client) {
    auto additional_providers = integration_client->GetAdditionalProviders();
    providers.insert(providers.end(),
                     make_move_iterator(additional_providers.begin()),
                     make_move_iterator(additional_providers.end()));
  }

  // Then add any other "built-in" providers
#if !defined(OS_ANDROID)
  providers.push_back(std::make_unique<IsolatedVRDeviceProvider>());
#endif  // !defined(OS_ANDROID)

  bool orientation_provider_enabled = true;

#if !defined(OS_ANDROID)
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  orientation_provider_enabled =
      IsEnabled(cmd_line, device::kWebXrOrientationSensorDevice,
                ::switches::kWebXrRuntimeOrientationSensors);
#endif

  if (orientation_provider_enabled) {
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    GetDeviceService().BindSensorProvider(
        sensor_provider.InitWithNewPipeAndPassReceiver());
    providers.emplace_back(
        std::make_unique<device::VROrientationDeviceProvider>(
            std::move(sensor_provider)));
  }
  return CreateInstance(std::move(providers));
}

void XRRuntimeManagerImpl::AddService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;

  // Loop through any currently active runtimes and send Connected messages to
  // the service. Future runtimes that come online will send a Connected message
  // when they are created.
  InitializeProviders();

  if (AreAllProvidersInitialized())
    service->InitializationComplete();

  services_.insert(service);
}

void XRRuntimeManagerImpl::RemoveService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  services_.erase(service);

  for (const auto& runtime : runtimes_) {
    runtime.second->OnServiceRemoved(service);
  }
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetRuntime(
    device::mojom::XRDeviceId id) {
  auto it = runtimes_.find(id);
  if (it == runtimes_.end())
    return nullptr;

  return it->second.get();
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetRuntimeForOptions(
    device::mojom::XRSessionOptions* options) {
  BrowserXRRuntimeImpl* runtime = nullptr;
  switch (options->mode) {
    case device::mojom::XRSessionMode::kImmersiveAr:
      runtime = GetRuntime(device::mojom::XRDeviceId::ARCORE_DEVICE_ID);
      break;
    case device::mojom::XRSessionMode::kImmersiveVr:
      runtime = GetImmersiveVrRuntime();
      break;
    case device::mojom::XRSessionMode::kInline:
      // Try the orientation provider if it exists.
      // If we don't have an orientation provider, then we don't have an
      // explicit runtime to back a non-immersive session.
      runtime = GetRuntime(device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID);
  }

  // Return the runtime from above if we got one and it supports all required
  // features.
  return runtime && runtime->SupportsAllFeatures(options->required_features)
             ? runtime
             : nullptr;
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetImmersiveVrRuntime() {
#if defined(OS_ANDROID)
  auto* gvr = GetRuntime(device::mojom::XRDeviceId::GVR_DEVICE_ID);
  if (gvr)
    return gvr;
#endif

#if BUILDFLAG(ENABLE_OPENXR)
  auto* openxr = GetRuntime(device::mojom::XRDeviceId::OPENXR_DEVICE_ID);
  if (openxr)
    return openxr;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  auto* openvr = GetRuntime(device::mojom::XRDeviceId::OPENVR_DEVICE_ID);
  if (openvr)
    return openvr;
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  auto* oculus = GetRuntime(device::mojom::XRDeviceId::OCULUS_DEVICE_ID);
  if (oculus)
    return oculus;
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  auto* wmr = GetRuntime(device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID);
  if (wmr)
    return wmr;
#endif

  return nullptr;
}

BrowserXRRuntimeImpl* XRRuntimeManagerImpl::GetImmersiveArRuntime() {
  device::mojom::XRSessionOptions options = {};
  options.mode = device::mojom::XRSessionMode::kImmersiveAr;
  return GetRuntimeForOptions(&options);
}

device::mojom::VRDisplayInfoPtr XRRuntimeManagerImpl::GetCurrentVRDisplayInfo(
    VRServiceImpl* service) {
  // This seems to be occurring every frame on Windows
  DVLOG(3) << __func__;
  // Get an immersive VR runtime if there is one.
  auto* immersive_runtime = GetImmersiveVrRuntime();
  if (immersive_runtime) {
    // Listen to changes for this runtime.
    immersive_runtime->OnServiceAdded(service);

    // If we don't have display info for the immersive runtime, get display info
    // from a different runtime.
    if (!immersive_runtime->GetVRDisplayInfo()) {
      immersive_runtime = nullptr;
    }
  }

  // Get an AR runtime if there is one.
  auto* ar_runtime = GetImmersiveArRuntime();
  if (ar_runtime) {
    // Listen to  changes for this runtime.
    ar_runtime->OnServiceAdded(service);
  }

  // If there is neither, use the generic non-immersive runtime.
  if (!ar_runtime && !immersive_runtime) {
    device::mojom::XRSessionOptions options = {};
    options.mode = device::mojom::XRSessionMode::kInline;
    auto* non_immersive_runtime = GetRuntimeForOptions(&options);
    if (non_immersive_runtime) {
      // Listen to changes for this runtime.
      non_immersive_runtime->OnServiceAdded(service);
    }

    // If we don't have an AR or immersive runtime, return the generic non-
    // immersive runtime's DisplayInfo if we have it.
    return non_immersive_runtime ? non_immersive_runtime->GetVRDisplayInfo()
                                 : nullptr;
  }

  // Use the immersive or AR runtime.
  device::mojom::VRDisplayInfoPtr device_info =
      immersive_runtime ? immersive_runtime->GetVRDisplayInfo()
                        : ar_runtime->GetVRDisplayInfo();

  return device_info;
}

BrowserXRRuntimeImpl*
XRRuntimeManagerImpl::GetCurrentlyPresentingImmersiveRuntime() {
  auto* vr_runtime = GetImmersiveVrRuntime();
  if (vr_runtime && vr_runtime->GetServiceWithActiveImmersiveSession()) {
    return vr_runtime;
  }

  auto* ar_runtime = GetImmersiveArRuntime();
  if (ar_runtime && ar_runtime->GetServiceWithActiveImmersiveSession()) {
    return ar_runtime;
  }

  return nullptr;
}

bool XRRuntimeManagerImpl::IsOtherClientPresenting(VRServiceImpl* service) {
  DCHECK(service);

  auto* runtime = GetCurrentlyPresentingImmersiveRuntime();
  if (!runtime)
    return false;  // No immersive runtime to be presenting.

  auto* presenting_service = runtime->GetServiceWithActiveImmersiveSession();

  // True if some other VRServiceImpl is presenting.
  return (presenting_service != service);
}

void XRRuntimeManagerImpl::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::SupportsSessionCallback callback) {
  auto* runtime = GetRuntimeForOptions(options.get());

  if (!runtime) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(http://crbug.com/842025): Pass supports session on to the runtimes.
  std::move(callback).Run(true);
}

XRRuntimeManagerImpl::XRRuntimeManagerImpl(XRProviderList providers)
    : providers_(std::move(providers)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!g_xr_runtime_manager);
  g_xr_runtime_manager = this;
}

XRRuntimeManagerImpl::~XRRuntimeManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(g_xr_runtime_manager, this);
  g_xr_runtime_manager = nullptr;
}

scoped_refptr<XRRuntimeManagerImpl> XRRuntimeManagerImpl::CreateInstance(
    XRProviderList providers) {
  auto* ptr = new XRRuntimeManagerImpl(std::move(providers));
  CHECK_EQ(ptr, g_xr_runtime_manager);
  return base::AdoptRef(ptr);
}

device::mojom::XRRuntime* XRRuntimeManagerImpl::GetRuntimeForTest(
    device::mojom::XRDeviceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeviceRuntimeMap::iterator iter =
      runtimes_.find(static_cast<device::mojom::XRDeviceId>(id));
  if (iter == runtimes_.end())
    return nullptr;

  return iter->second->GetRuntime();
}

size_t XRRuntimeManagerImpl::NumberOfConnectedServices() {
  return services_.size();
}

void XRRuntimeManagerImpl::InitializeProviders() {
  if (providers_initialized_)
    return;

  for (const auto& provider : providers_) {
    if (!provider) {
      // TODO(crbug.com/1050470): Remove this logging after investigation.
      LOG(ERROR) << __func__ << " got null XR provider";
      continue;
    }

    provider->Initialize(
        base::BindRepeating(&XRRuntimeManagerImpl::AddRuntime,
                            base::Unretained(this)),
        base::BindRepeating(&XRRuntimeManagerImpl::RemoveRuntime,
                            base::Unretained(this)),
        base::BindOnce(&XRRuntimeManagerImpl::OnProviderInitialized,
                       base::Unretained(this)));
  }

  providers_initialized_ = true;
}

void XRRuntimeManagerImpl::OnProviderInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++num_initialized_providers_;
  if (AreAllProvidersInitialized()) {
    for (VRServiceImpl* service : services_)
      service->InitializationComplete();
  }
}

bool XRRuntimeManagerImpl::AreAllProvidersInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return num_initialized_providers_ == providers_.size();
}

void XRRuntimeManagerImpl::AddRuntime(
    device::mojom::XRDeviceId id,
    device::mojom::VRDisplayInfoPtr info,
    mojo::PendingRemote<device::mojom::XRRuntime> runtime) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(runtimes_.find(id) == runtimes_.end());

  TRACE_EVENT_INSTANT1("xr", "AddRuntime", TRACE_EVENT_SCOPE_THREAD, "id", id);

  runtimes_[id] = std::make_unique<BrowserXRRuntimeImpl>(id, std::move(runtime),
                                                         std::move(info));

  for (Observer& obs : g_xr_runtime_manager_observers.Get())
    obs.OnRuntimeAdded(runtimes_[id].get());

  for (VRServiceImpl* service : services_)
    // TODO(sumankancherla): Consider combining with XRRuntimeManager::Observer.
    service->RuntimesChanged();
}

void XRRuntimeManagerImpl::RemoveRuntime(device::mojom::XRDeviceId id) {
  DVLOG(1) << __func__ << " id: " << id;
  TRACE_EVENT_INSTANT1("xr", "RemoveRuntime", TRACE_EVENT_SCOPE_THREAD, "id",
                       id);

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = runtimes_.find(id);
  DCHECK(it != runtimes_.end());

  // Give the runtime a chance to clean itself up before notifying services
  // that it was removed.
  it->second->BeforeRuntimeRemoved();

  // Remove the runtime from runtimes_ before notifying services that it was
  // removed, since they will query for runtimes in RuntimesChanged.
  std::unique_ptr<BrowserXRRuntimeImpl> removed_runtime = std::move(it->second);
  runtimes_.erase(it);

  for (VRServiceImpl* service : services_)
    service->RuntimesChanged();
}

void XRRuntimeManagerImpl::ForEachRuntime(
    base::RepeatingCallback<void(BrowserXRRuntime*)> fn) {
  for (auto& runtime : runtimes_) {
    fn.Run(runtime.second.get());
  }
}

}  // namespace content
