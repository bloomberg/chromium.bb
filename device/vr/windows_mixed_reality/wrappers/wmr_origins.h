// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_ORIGINS_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_ORIGINS_H_

#include <windows.perception.spatial.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"

namespace device {
class WMRTimestamp;

class WMRCoordinateSystem {
 public:
  explicit WMRCoordinateSystem(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
          coordinates);
  virtual ~WMRCoordinateSystem();

  bool TryGetTransformTo(
      const WMRCoordinateSystem* other,
      ABI::Windows::Foundation::Numerics::Matrix4x4* this_to_other);

  ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem* GetRawPtr()
      const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
      coordinates_;

  DISALLOW_COPY_AND_ASSIGN(WMRCoordinateSystem);
};

class WMRStationaryOrigin {
 public:
  static std::unique_ptr<WMRStationaryOrigin> CreateAtCurrentLocation();
  explicit WMRStationaryOrigin(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference>
          stationary_origin);
  virtual ~WMRStationaryOrigin();

  std::unique_ptr<WMRCoordinateSystem> CoordinateSystem();

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference>
      stationary_origin_;

  DISALLOW_COPY_AND_ASSIGN(WMRStationaryOrigin);
};

class WMRAttachedOrigin {
 public:
  static std::unique_ptr<WMRAttachedOrigin> CreateAtCurrentLocation();
  explicit WMRAttachedOrigin(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                                 ISpatialLocatorAttachedFrameOfReference>
          attached_origin);
  virtual ~WMRAttachedOrigin();

  std::unique_ptr<WMRCoordinateSystem> TryGetCoordinatesAtTimestamp(
      const WMRTimestamp* timestamp);

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                             ISpatialLocatorAttachedFrameOfReference>
      attached_origin_;

  DISALLOW_COPY_AND_ASSIGN(WMRAttachedOrigin);
};

class WMRStageOrigin {
 public:
  static std::unique_ptr<WMRStageOrigin> CreateAtCurrentLocation();
  explicit WMRStageOrigin(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference>
          stage_origin);
  virtual ~WMRStageOrigin();

  std::unique_ptr<WMRCoordinateSystem> CoordinateSystem();
  ABI::Windows::Perception::Spatial::SpatialMovementRange MovementRange();

  // This will return an empty array if no bounds are set.
  std::vector<ABI::Windows::Foundation::Numerics::Vector3> GetMovementBounds(
      const WMRCoordinateSystem* coordinates);

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference>
      stage_origin_;

  DISALLOW_COPY_AND_ASSIGN(WMRStageOrigin);
};

class WMRStageStatics {
 public:
  static std::unique_ptr<WMRStageStatics> Create();
  explicit WMRStageStatics(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                                 ISpatialStageFrameOfReferenceStatics>
          stage_statics);
  virtual ~WMRStageStatics();

  std::unique_ptr<WMRStageOrigin> CurrentStage();

  std::unique_ptr<base::CallbackList<void()>::Subscription>
  AddStageChangedCallback(const base::RepeatingCallback<void()>& cb);

 private:
  HRESULT OnCurrentChanged(IInspectable* sender, IInspectable* args);
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReferenceStatics>
      stage_statics_;

  EventRegistrationToken stage_changed_token_;
  base::CallbackList<void()> callback_list_;

  DISALLOW_COPY_AND_ASSIGN(WMRStageStatics);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_ORIGINS_H_
