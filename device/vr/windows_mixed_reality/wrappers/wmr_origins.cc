// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"

#include <windows.perception.spatial.h>
#include <wrl.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using SpatialMovementRange =
    ABI::Windows::Perception::Spatial::SpatialMovementRange;
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
// WMRCoordinateSystem
WMRCoordinateSystem::WMRCoordinateSystem(
    ComPtr<ISpatialCoordinateSystem> coordinates)
    : coordinates_(coordinates) {
  DCHECK(coordinates_);
}

WMRCoordinateSystem::~WMRCoordinateSystem() = default;

bool WMRCoordinateSystem::TryGetTransformTo(const WMRCoordinateSystem* other,
                                            WFN::Matrix4x4* this_to_other) {
  DCHECK(this_to_other);
  DCHECK(other);
  ComPtr<IReference<WFN::Matrix4x4>> this_to_other_ref;
  HRESULT hr =
      coordinates_->TryGetTransformTo(other->GetRawPtr(), &this_to_other_ref);
  if (FAILED(hr) || !this_to_other_ref) {
    WMRLogging::TraceError(WMRErrorLocation::kGetTransformBetweenOrigins, hr);
    return false;
  }

  hr = this_to_other_ref->get_Value(this_to_other);
  DCHECK(SUCCEEDED(hr));
  return true;
}

ISpatialCoordinateSystem* WMRCoordinateSystem::GetRawPtr() const {
  return coordinates_.Get();
}

// WMRStationaryOrigin
std::unique_ptr<WMRStationaryOrigin>
WMRStationaryOrigin::CreateAtCurrentLocation() {
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

  return std::make_unique<WMRStationaryOrigin>(origin);
}

WMRStationaryOrigin::WMRStationaryOrigin(
    ComPtr<ISpatialStationaryFrameOfReference> stationary_origin)
    : stationary_origin_(stationary_origin) {
  DCHECK(stationary_origin_);
}

WMRStationaryOrigin::~WMRStationaryOrigin() = default;

std::unique_ptr<WMRCoordinateSystem> WMRStationaryOrigin::CoordinateSystem() {
  ComPtr<ISpatialCoordinateSystem> coordinates;
  HRESULT hr = stationary_origin_->get_CoordinateSystem(&coordinates);
  DCHECK(SUCCEEDED(hr));
  return std::make_unique<WMRCoordinateSystem>(coordinates);
}

// WMRAttachedOrigin
std::unique_ptr<WMRAttachedOrigin>
WMRAttachedOrigin::CreateAtCurrentLocation() {
  ComPtr<ISpatialLocator> locator = GetSpatialLocator();
  if (!locator)
    return nullptr;

  ComPtr<ISpatialLocatorAttachedFrameOfReference> origin;
  HRESULT hr = locator->CreateAttachedFrameOfReferenceAtCurrentHeading(&origin);
  if (FAILED(hr)) {
    WMRLogging::TraceError(WMRErrorLocation::kAttachedReferenceCreation, hr);
    return nullptr;
  }

  return std::make_unique<WMRAttachedOrigin>(origin);
}

WMRAttachedOrigin::WMRAttachedOrigin(
    ComPtr<ISpatialLocatorAttachedFrameOfReference> attached_origin)
    : attached_origin_(attached_origin) {
  DCHECK(attached_origin_);
}

WMRAttachedOrigin::~WMRAttachedOrigin() = default;

std::unique_ptr<WMRCoordinateSystem>
WMRAttachedOrigin::TryGetCoordinatesAtTimestamp(const WMRTimestamp* timestamp) {
  ComPtr<ISpatialCoordinateSystem> coordinates;
  HRESULT hr = attached_origin_->GetStationaryCoordinateSystemAtTimestamp(
      timestamp->GetRawPtr(), &coordinates);
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRCoordinateSystem>(coordinates);
}

// WMRStageOrigin
WMRStageOrigin::WMRStageOrigin(
    ComPtr<ISpatialStageFrameOfReference> stage_origin)
    : stage_origin_(stage_origin) {
  DCHECK(stage_origin_);
}

WMRStageOrigin::~WMRStageOrigin() = default;

std::unique_ptr<WMRCoordinateSystem> WMRStageOrigin::CoordinateSystem() {
  ComPtr<ISpatialCoordinateSystem> coordinates;
  HRESULT hr = stage_origin_->get_CoordinateSystem(&coordinates);
  DCHECK(SUCCEEDED(hr));

  return std::make_unique<WMRCoordinateSystem>(coordinates);
}

SpatialMovementRange WMRStageOrigin::MovementRange() {
  SpatialMovementRange movement_range;
  HRESULT hr = stage_origin_->get_MovementRange(&movement_range);
  DCHECK(SUCCEEDED(hr));

  return movement_range;
}

std::vector<WFN::Vector3> WMRStageOrigin::GetMovementBounds(
    const WMRCoordinateSystem* coordinates) {
  DCHECK(coordinates);

  std::vector<WFN::Vector3> ret_val;
  uint32_t size;
  base::win::ScopedCoMem<WFN::Vector3> bounds;
  HRESULT hr = stage_origin_->TryGetMovementBounds(coordinates->GetRawPtr(),
                                                   &size, &bounds);
  if (FAILED(hr))
    return ret_val;

  for (uint32_t i = 0; i < size; i++) {
    ret_val.push_back(bounds[i]);
  }

  return ret_val;
}

// WMRStageStatics
std::unique_ptr<WMRStageStatics> WMRStageStatics::Create() {
  ComPtr<ISpatialStageFrameOfReferenceStatics> stage_statics;
  base::win::ScopedHString spatial_stage_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialStageFrameOfReference);
  HRESULT hr = base::win::RoGetActivationFactory(spatial_stage_string.get(),
                                                 IID_PPV_ARGS(&stage_statics));
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRStageStatics>(stage_statics);
}

WMRStageStatics::WMRStageStatics(
    ComPtr<ISpatialStageFrameOfReferenceStatics> stage_statics)
    : stage_statics_(stage_statics) {
  DCHECK(stage_statics_);
}

WMRStageStatics::~WMRStageStatics() = default;

std::unique_ptr<WMRStageOrigin> WMRStageStatics::CurrentStage() {
  ComPtr<ISpatialStageFrameOfReference> stage_origin;
  HRESULT hr = stage_statics_->get_Current(&stage_origin);
  if (FAILED(hr) || !stage_origin) {
    WMRLogging::TraceError(WMRErrorLocation::kAcquireCurrentStage, hr);
    return nullptr;
  }

  return std::make_unique<WMRStageOrigin>(stage_origin);
}

ComPtr<ISpatialStageFrameOfReferenceStatics> WMRStageStatics::GetComPtr()
    const {
  return stage_statics_;
}
}  // namespace device
