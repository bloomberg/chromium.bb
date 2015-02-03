// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_candidate.h"

#include <algorithm>
#include "base/logging.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

OverlayCandidate::OverlayCandidate()
    : transform(gfx::OVERLAY_TRANSFORM_NONE),
      format(RGBA_8888),
      uv_rect(0.f, 0.f, 1.f, 1.f),
      resource_id(0),
      plane_z_order(0),
      overlay_handled(false) {}

OverlayCandidate::~OverlayCandidate() {}

// static
gfx::OverlayTransform OverlayCandidate::GetOverlayTransform(
    const gfx::Transform& quad_transform,
    bool flipped) {
  if (!quad_transform.IsPositiveScaleOrTranslation())
    return gfx::OVERLAY_TRANSFORM_INVALID;

  return flipped ? gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL
                 : gfx::OVERLAY_TRANSFORM_NONE;
}

// static
gfx::OverlayTransform OverlayCandidate::ModifyTransform(
    gfx::OverlayTransform in,
    gfx::OverlayTransform delta) {
  // There are 8 different possible transforms. We can characterize these
  // by looking at where the origin moves and the direction the horizontal goes.
  // (TL=top-left, BR=bottom-right, H=horizontal, V=vertical).
  // NONE: TL, H
  // FLIP_VERTICAL: BL, H
  // FLIP_HORIZONTAL: TR, H
  // ROTATE_90: TR, V
  // ROTATE_180: BR, H
  // ROTATE_270: BL, V
  // Missing transforms: TL, V & BR, V
  // Basic combinations:
  // Flip X & Y -> Rotate 180 (TL,H -> TR,H -> BR,H or TL,H -> BL,H -> BR,H)
  // Flip X or Y + Rotate 180 -> other flip (eg, TL,H -> TR,H -> BL,H)
  // Rotate + Rotate simply adds values.
  // Rotate 90/270 + flip is invalid because we can only have verticals with
  // the origin in TR or BL.
  if (delta == gfx::OVERLAY_TRANSFORM_NONE)
    return in;
  switch (in) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return delta;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_ROTATE_270;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
          return gfx::OVERLAY_TRANSFORM_NONE;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
          return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
        case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
          return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
          return gfx::OVERLAY_TRANSFORM_ROTATE_270;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
          return gfx::OVERLAY_TRANSFORM_ROTATE_90;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      switch (delta) {
        case gfx::OVERLAY_TRANSFORM_ROTATE_90:
          return gfx::OVERLAY_TRANSFORM_NONE;
        case gfx::OVERLAY_TRANSFORM_ROTATE_180:
          return gfx::OVERLAY_TRANSFORM_ROTATE_90;
        case gfx::OVERLAY_TRANSFORM_ROTATE_270:
          return gfx::OVERLAY_TRANSFORM_ROTATE_180;
        default:
          return gfx::OVERLAY_TRANSFORM_INVALID;
      }
      break;
    default:
      return gfx::OVERLAY_TRANSFORM_INVALID;
  }
}

// static
gfx::Rect OverlayCandidate::GetOverlayRect(const gfx::Transform& quad_transform,
                                           const gfx::Rect& rect) {
  DCHECK(quad_transform.IsPositiveScaleOrTranslation());

  gfx::RectF float_rect(rect);
  quad_transform.TransformRect(&float_rect);
  return gfx::ToNearestRect(float_rect);
}

}  // namespace cc
