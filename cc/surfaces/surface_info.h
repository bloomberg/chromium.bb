// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_INFO_H_
#define CC_SURFACES_SURFACE_INFO_H_

#include "cc/surfaces/surface_id.h"
#include "ui/gfx/geometry/size.h"

namespace IPC {
template <class T>
struct ParamTraits;
}  // namespace IPC

namespace cc {
namespace mojom {
class SurfaceInfoDataView;
}

// This class contains information about the surface that is being embedded.
class SurfaceInfo {
 public:
  SurfaceInfo() = default;
  SurfaceInfo(const SurfaceId& id,
              float device_scale_factor,
              const gfx::Size& size_in_pixels)
      : id_(id),
        device_scale_factor_(device_scale_factor),
        size_in_pixels_(size_in_pixels) {}

  bool is_valid() const {
    return id_.is_valid() && device_scale_factor_ != 0 &&
           !size_in_pixels_.IsEmpty();
  }

  bool operator==(const SurfaceInfo& info) const {
    return id_ == info.id() &&
           device_scale_factor_ == info.device_scale_factor() &&
           size_in_pixels_ == info.size_in_pixels();
  }

  bool operator!=(const SurfaceInfo& info) const { return !(*this == info); }

  const SurfaceId& id() const { return id_; }
  float device_scale_factor() const { return device_scale_factor_; }
  const gfx::Size& size_in_pixels() const { return size_in_pixels_; }

 private:
  friend struct mojo::StructTraits<mojom::SurfaceInfoDataView, SurfaceInfo>;
  friend struct IPC::ParamTraits<SurfaceInfo>;

  SurfaceId id_;
  float device_scale_factor_ = 1.f;
  gfx::Size size_in_pixels_;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_INFO_H_
