// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h"

#include <windows.perception.spatial.h>
#include <wrl.h>

#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_logging.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using SpatialMovementRange =
    ABI::Windows::Perception::Spatial::SpatialMovementRange;
using ABI::Windows::Foundation::IEventHandler;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem;
using ABI::Windows::Perception::Spatial::ISpatialLocator;
using ABI::Windows::Perception::Spatial::
    ISpatialLocatorAttachedFrameOfReference;
using ABI::Windows::Perception::Spatial::ISpatialLocatorStatics;
using ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference;
using ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReferenceStatics;
using ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference;
using Microsoft::WRL::ComPtr;

namespace {
ComPtr<ISpatialLocator> GetSpatialLocator() {
  ComPtr<ISpatialLocatorStatics> spatial_locator_statics;
  base::win::ScopedHString spatial_locator_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialLocator);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_locator_string.get(), IID_PPV_ARGS(&spatial_locator_statics));
  if (FAILED(hr)) {
    device::WMRLogging::TraceError(device::WMRErrorLocation::kGetSpatialLocator,
                                   hr);
    return nullptr;
  }

  ComPtr<ISpatialLocator> locator;
  hr = spatial_locator_statics->GetDefault(&locator);
  if (FAILED(hr)) {
    device::WMRLogging::TraceError(device::WMRErrorLocation::kGetSpatialLocator,
                                   hr);
    return nullptr;
  }

  return locator;
}
}  // anonymous namespace

namespace device {

std::unique_ptr<WMRStationaryOrigin>
WMRStationaryOriginFactory::CreateAtCurrentLocation() {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRStationaryOrigin>();
  }

  ComPtr<ISpatialLocator> locator = GetSpatialLocator();
  if (!locator)
    return nullptr;

  ComPtr<ISpatialStationaryFrameOfReference> origin;
  HRESULT hr =
      locator->CreateStationaryFrameOfReferenceAtCurrentLocation(&origin);
  if (FAILED(hr)) {
    WMRLogging::TraceError(WMRErrorLocation::kStationaryReferenceCreation, hr);
    return nullptr;
  }

  return std::make_unique<WMRStationaryOriginImpl>(origin);
}

std::unique_ptr<WMRAttachedOrigin>
WMRAttachedOriginFactory::CreateAtCurrentLocation() {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRAttachedOrigin>();
  }

  ComPtr<ISpatialLocator> locator = GetSpatialLocator();
  if (!locator)
    return nullptr;

  ComPtr<ISpatialLocatorAttachedFrameOfReference> origin;
  HRESULT hr = locator->CreateAttachedFrameOfReferenceAtCurrentHeading(&origin);
  if (FAILED(hr)) {
    WMRLogging::TraceError(WMRErrorLocation::kAttachedReferenceCreation, hr);
    return nullptr;
  }

  return std::make_unique<WMRAttachedOriginImpl>(origin);
}

std::unique_ptr<WMRStageStatics> WMRStageStaticsFactory::Create() {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRStageStatics>();
  }

  ComPtr<ISpatialStageFrameOfReferenceStatics> stage_statics;
  base::win::ScopedHString spatial_stage_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialStageFrameOfReference);
  HRESULT hr = base::win::RoGetActivationFactory(spatial_stage_string.get(),
                                                 IID_PPV_ARGS(&stage_statics));
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRStageStaticsImpl>(stage_statics);
}

}  // namespace device
