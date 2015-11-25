// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_proto_factory.h"

#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/proto/display_item.pb.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// static
DisplayItem* DisplayItemProtoFactory::AllocateAndConstruct(
    const gfx::Rect& visual_rect,
    DisplayItemList* list,
    const proto::DisplayItem& proto) {
  switch (proto.type()) {
    case proto::DisplayItem::Type_Clip:
      return list->CreateAndAppendItem<ClipDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_EndClip:
      return list->CreateAndAppendItem<EndClipDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_ClipPath:
      return list->CreateAndAppendItem<ClipPathDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_EndClipPath:
      return list->CreateAndAppendItem<EndClipPathDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_Compositing:
      return list->CreateAndAppendItem<CompositingDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_EndCompositing:
      return list->CreateAndAppendItem<EndCompositingDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_Drawing:
      return list->CreateAndAppendItem<DrawingDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_Filter:
      return list->CreateAndAppendItem<FilterDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_EndFilter:
      return list->CreateAndAppendItem<EndFilterDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_FloatClip:
      return list->CreateAndAppendItem<FloatClipDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_EndFloatClip:
      return list->CreateAndAppendItem<EndFloatClipDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_Transform:
      return list->CreateAndAppendItem<TransformDisplayItem>(visual_rect);
    case proto::DisplayItem::Type_EndTransform:
      return list->CreateAndAppendItem<EndTransformDisplayItem>(visual_rect);
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace cc
