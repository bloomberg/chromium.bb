// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <cstdint>

#include "base/logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_source_pose.h"

using SourceHandedness =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness;
using SourceKind =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind;

using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource;
using Microsoft::WRL::ComPtr;

namespace device {

ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource*
WMRInputSource::GetRawPtr() const {
  // This should only ever be used by the real implementation, so by default
  // make sure it's not called.
  NOTREACHED();
  return nullptr;
}

WMRInputSourceImpl::WMRInputSourceImpl(ComPtr<ISpatialInteractionSource> source)
    : source_(source) {
  DCHECK(source_);
  source_.As(&source2_);
  source_.As(&source3_);
}

WMRInputSourceImpl::~WMRInputSourceImpl() = default;

WMRInputSourceImpl::WMRInputSourceImpl(const WMRInputSourceImpl& other) =
    default;

uint32_t WMRInputSourceImpl::Id() const {
  uint32_t val;
  HRESULT hr = source_->get_Id(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

SourceKind WMRInputSourceImpl::Kind() const {
  SourceKind val;
  HRESULT hr = source_->get_Kind(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRInputSourceImpl::IsPointingSupported() const {
  if (!source2_)
    return false;
  boolean val;
  HRESULT hr = source2_->get_IsPointingSupported(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

SourceHandedness WMRInputSourceImpl::Handedness() const {
  if (!source3_)
    return SourceHandedness::SpatialInteractionSourceHandedness_Unspecified;
  SourceHandedness val;
  HRESULT hr = source3_->get_Handedness(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

ISpatialInteractionSource* WMRInputSourceImpl::GetRawPtr() const {
  return source_.Get();
}
}  // namespace device
