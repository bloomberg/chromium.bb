// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/skia_conversions.h"

#include "base/logging.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/skclipop.pb.h"
#include "cc/proto/skrrect.pb.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

void SkPointToProto(const SkPoint& point, proto::PointF* proto) {
  PointFToProto(gfx::PointF(point.x(), point.y()), proto);
}

SkPoint ProtoToSkPoint(const proto::PointF& proto) {
  gfx::PointF point = ProtoToPointF(proto);
  return SkPoint::Make(point.x(), point.y());
}

}  // namespace

SkBlendMode SkXfermodeModeFromProto(proto::SkXfermode::Mode mode) {
  switch (mode) {
    case proto::SkXfermode::CLEAR_:
      return SkBlendMode::kClear;
    case proto::SkXfermode::SRC:
      return SkBlendMode::kSrc;
    case proto::SkXfermode::DST:
      return SkBlendMode::kDst;
    case proto::SkXfermode::SRC_OVER:
      return SkBlendMode::kSrcOver;
    case proto::SkXfermode::DST_OVER:
      return SkBlendMode::kDstOver;
    case proto::SkXfermode::SRC_IN:
      return SkBlendMode::kSrcIn;
    case proto::SkXfermode::DST_IN:
      return SkBlendMode::kDstIn;
    case proto::SkXfermode::SRC_OUT:
      return SkBlendMode::kSrcOut;
    case proto::SkXfermode::DST_OUT:
      return SkBlendMode::kDstOut;
    case proto::SkXfermode::SRC_A_TOP:
      return SkBlendMode::kSrcATop;
    case proto::SkXfermode::DST_A_TOP:
      return SkBlendMode::kDstATop;
    case proto::SkXfermode::XOR:
      return SkBlendMode::kXor;
    case proto::SkXfermode::PLUS:
      return SkBlendMode::kPlus;
    case proto::SkXfermode::MODULATE:
      return SkBlendMode::kModulate;
    case proto::SkXfermode::SCREEN:
      return SkBlendMode::kScreen;
    case proto::SkXfermode::OVERLAY:
      return SkBlendMode::kOverlay;
    case proto::SkXfermode::DARKEN:
      return SkBlendMode::kDarken;
    case proto::SkXfermode::LIGHTEN:
      return SkBlendMode::kLighten;
    case proto::SkXfermode::COLOR_DODGE:
      return SkBlendMode::kColorDodge;
    case proto::SkXfermode::COLOR_BURN:
      return SkBlendMode::kColorBurn;
    case proto::SkXfermode::HARD_LIGHT:
      return SkBlendMode::kHardLight;
    case proto::SkXfermode::SOFT_LIGHT:
      return SkBlendMode::kSoftLight;
    case proto::SkXfermode::DIFFERENCE_:
      return SkBlendMode::kDifference;
    case proto::SkXfermode::EXCLUSION:
      return SkBlendMode::kExclusion;
    case proto::SkXfermode::MULTIPLY:
      return SkBlendMode::kMultiply;
    case proto::SkXfermode::HUE:
      return SkBlendMode::kHue;
    case proto::SkXfermode::SATURATION:
      return SkBlendMode::kSaturation;
    case proto::SkXfermode::COLOR:
      return SkBlendMode::kColor;
    case proto::SkXfermode::LUMINOSITY:
      return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kClear;
}

proto::SkXfermode::Mode SkXfermodeModeToProto(SkBlendMode mode) {
  switch (mode) {
    case SkBlendMode::kClear:
      return proto::SkXfermode::CLEAR_;
    case SkBlendMode::kSrc:
      return proto::SkXfermode::SRC;
    case SkBlendMode::kDst:
      return proto::SkXfermode::DST;
    case SkBlendMode::kSrcOver:
      return proto::SkXfermode::SRC_OVER;
    case SkBlendMode::kDstOver:
      return proto::SkXfermode::DST_OVER;
    case SkBlendMode::kSrcIn:
      return proto::SkXfermode::SRC_IN;
    case SkBlendMode::kDstIn:
      return proto::SkXfermode::DST_IN;
    case SkBlendMode::kSrcOut:
      return proto::SkXfermode::SRC_OUT;
    case SkBlendMode::kDstOut:
      return proto::SkXfermode::DST_OUT;
    case SkBlendMode::kSrcATop:
      return proto::SkXfermode::SRC_A_TOP;
    case SkBlendMode::kDstATop:
      return proto::SkXfermode::DST_A_TOP;
    case SkBlendMode::kXor:
      return proto::SkXfermode::XOR;
    case SkBlendMode::kPlus:
      return proto::SkXfermode::PLUS;
    case SkBlendMode::kModulate:
      return proto::SkXfermode::MODULATE;
    case SkBlendMode::kScreen:
      return proto::SkXfermode::SCREEN;
    case SkBlendMode::kOverlay:
      return proto::SkXfermode::OVERLAY;
    case SkBlendMode::kDarken:
      return proto::SkXfermode::DARKEN;
    case SkBlendMode::kLighten:
      return proto::SkXfermode::LIGHTEN;
    case SkBlendMode::kColorDodge:
      return proto::SkXfermode::COLOR_DODGE;
    case SkBlendMode::kColorBurn:
      return proto::SkXfermode::COLOR_BURN;
    case SkBlendMode::kHardLight:
      return proto::SkXfermode::HARD_LIGHT;
    case SkBlendMode::kSoftLight:
      return proto::SkXfermode::SOFT_LIGHT;
    case SkBlendMode::kDifference:
      return proto::SkXfermode::DIFFERENCE_;
    case SkBlendMode::kExclusion:
      return proto::SkXfermode::EXCLUSION;
    case SkBlendMode::kMultiply:
      return proto::SkXfermode::MULTIPLY;
    case SkBlendMode::kHue:
      return proto::SkXfermode::HUE;
    case SkBlendMode::kSaturation:
      return proto::SkXfermode::SATURATION;
    case SkBlendMode::kColor:
      return proto::SkXfermode::COLOR;
    case SkBlendMode::kLuminosity:
      return proto::SkXfermode::LUMINOSITY;
  }
  return proto::SkXfermode::CLEAR_;
}

void SkRRectToProto(const SkRRect& rect, proto::SkRRect* proto) {
  RectFToProto(gfx::SkRectToRectF(rect.rect()), proto->mutable_rect());

  SkPointToProto(rect.radii(SkRRect::kUpperLeft_Corner),
                 proto->mutable_radii_upper_left());
  SkPointToProto(rect.radii(SkRRect::kUpperRight_Corner),
                 proto->mutable_radii_upper_right());
  SkPointToProto(rect.radii(SkRRect::kLowerRight_Corner),
                 proto->mutable_radii_lower_right());
  SkPointToProto(rect.radii(SkRRect::kLowerLeft_Corner),
                 proto->mutable_radii_lower_left());
}

SkRRect ProtoToSkRRect(const proto::SkRRect& proto) {
  SkRect parsed_rect = gfx::RectFToSkRect(ProtoToRectF(proto.rect()));
  SkVector parsed_radii[4];
  parsed_radii[SkRRect::kUpperLeft_Corner] =
      ProtoToSkPoint(proto.radii_upper_left());
  parsed_radii[SkRRect::kUpperRight_Corner] =
      ProtoToSkPoint(proto.radii_upper_right());
  parsed_radii[SkRRect::kLowerRight_Corner] =
      ProtoToSkPoint(proto.radii_lower_right());
  parsed_radii[SkRRect::kLowerLeft_Corner] =
      ProtoToSkPoint(proto.radii_lower_left());
  SkRRect rect;
  rect.setRectRadii(parsed_rect, parsed_radii);
  return rect;
}

}  // namespace cc
