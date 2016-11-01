// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/cc_conversions.h"

#include "cc/base/region.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/region.pb.h"

namespace cc {

void RegionToProto(const Region& region, proto::Region* proto) {
  for (Region::Iterator it(region); it.has_rect(); it.next())
    RectToProto(it.rect(), proto->add_rects());
}

Region RegionFromProto(const proto::Region& proto) {
  Region region;
  for (int i = 0; i < proto.rects_size(); ++i)
    region.Union(ProtoToRect(proto.rects(i)));
  return region;
}

proto::SolidColorScrollbarLayerProperties::ScrollbarOrientation
ScrollbarOrientationToProto(const ScrollbarOrientation& orientation) {
  switch (orientation) {
    case ScrollbarOrientation::HORIZONTAL:
      return proto::SolidColorScrollbarLayerProperties::HORIZONTAL;
    case ScrollbarOrientation::VERTICAL:
      return proto::SolidColorScrollbarLayerProperties::VERTICAL;
  }
  return proto::SolidColorScrollbarLayerProperties::HORIZONTAL;
}

ScrollbarOrientation ScrollbarOrientationFromProto(
    const proto::SolidColorScrollbarLayerProperties::ScrollbarOrientation&
        proto) {
  switch (proto) {
    case proto::SolidColorScrollbarLayerProperties::HORIZONTAL:
      return ScrollbarOrientation::HORIZONTAL;
    case proto::SolidColorScrollbarLayerProperties::VERTICAL:
      return ScrollbarOrientation::VERTICAL;
  }
  return ScrollbarOrientation::HORIZONTAL;
}

proto::ClipNodeData::ClipType ClipNodeTypeToProto(
    const ClipNode::ClipType& clip_type) {
  switch (clip_type) {
    case ClipNode::ClipType::NONE:
      return proto::ClipNodeData::NONE;
    case ClipNode::ClipType::APPLIES_LOCAL_CLIP:
      return proto::ClipNodeData::APPLIES_LOCAL_CLIP;
    case ClipNode::ClipType::EXPANDS_CLIP:
      return proto::ClipNodeData::EXPANDS_CLIP;
  }
  NOTREACHED();
  return proto::ClipNodeData::NONE;
}

ClipNode::ClipType ClipNodeTypeFromProto(
    const proto::ClipNodeData::ClipType& clip_type) {
  switch (clip_type) {
    case proto::ClipNodeData::NONE:
      return ClipNode::ClipType::NONE;
    case proto::ClipNodeData::APPLIES_LOCAL_CLIP:
      return ClipNode::ClipType::APPLIES_LOCAL_CLIP;
    case proto::ClipNodeData::EXPANDS_CLIP:
      return ClipNode::ClipType::EXPANDS_CLIP;
  }
  NOTREACHED();
  return ClipNode::ClipType::NONE;
}

}  // namespace cc
