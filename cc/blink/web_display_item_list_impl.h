// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
#define CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/public/platform/web_display_item_list.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/point_f.h"

class SkColorFilter;
class SkMatrix44;
class SkPath;
class SkRRect;

namespace cc {
class FilterOperations;
class PaintOpBuffer;
}

namespace cc_blink {

class CC_BLINK_EXPORT WebDisplayItemListImpl
    : public blink::WebDisplayItemList {
 public:
  WebDisplayItemListImpl();
  explicit WebDisplayItemListImpl(cc::DisplayItemList* display_list);
  ~WebDisplayItemListImpl() override;

  // blink::WebDisplayItemList implementation.
  void AppendDrawingItem(const gfx::Rect& visual_rect,
                         sk_sp<const cc::PaintOpBuffer> record) override;
  void AppendClipItem(
      const gfx::Rect& clip_rect,
      const blink::WebVector<SkRRect>& rounded_clip_rects) override;
  void AppendEndClipItem() override;
  void AppendClipPathItem(const SkPath& clip_path, bool antialias) override;
  void AppendEndClipPathItem() override;
  void AppendFloatClipItem(const gfx::RectF& clip_rect) override;
  void AppendEndFloatClipItem() override;
  void AppendTransformItem(const SkMatrix44& matrix) override;
  void AppendEndTransformItem() override;
  void AppendCompositingItem(float opacity,
                             SkBlendMode,
                             SkRect* bounds,
                             SkColorFilter*) override;
  void AppendEndCompositingItem() override;
  void AppendFilterItem(const cc::FilterOperations& filters,
                        const gfx::RectF& filter_bounds,
                        const gfx::PointF& origin) override;
  void AppendEndFilterItem() override;
  void AppendScrollItem(const gfx::Vector2d& scroll_offset,
                        ScrollContainerId) override;
  void AppendEndScrollItem() override;
  cc::DisplayItemList* GetCcDisplayItemList() override;

 private:
  void AppendRestore();

  scoped_refptr<cc::DisplayItemList> display_item_list_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayItemListImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
