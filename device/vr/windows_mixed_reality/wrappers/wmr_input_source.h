// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_H_

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <cstdint>

namespace device {
class WMRInputSource {
 public:
  explicit WMRInputSource(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource> source);
  WMRInputSource(const WMRInputSource& other);
  virtual ~WMRInputSource();

  // Uses ISpatialInteractionSource.
  uint32_t Id() const;
  ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind Kind() const;

  // Uses ISpatialInteractionSource2.
  bool IsPointingSupported() const;

  // Uses ISpatialInteractionSource3.
  ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness
  Handedness() const;

  ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource* GetRawPtr()
      const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource>
      source_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource2>
      source2_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource3>
      source3_;
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_H_
