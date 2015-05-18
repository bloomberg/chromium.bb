// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
#define CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_

#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/playback/display_item_list.h"
#include "third_party/WebKit/public/platform/WebDisplayItemList.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/point_f.h"

class SkColorFilter;
class SkImageFilter;
class SkMatrix44;
class SkPath;
class SkPicture;
class SkRRect;

namespace blink {
class WebFilterOperations;
struct WebFloatRect;
struct WebRect;
}

namespace cc_blink {

class WebDisplayItemListImpl : public blink::WebDisplayItemList {
 public:
  CC_BLINK_EXPORT WebDisplayItemListImpl(cc::DisplayItemList* display_list);
  virtual ~WebDisplayItemListImpl();

  // blink::WebDisplayItemList implementation.
  virtual void appendDrawingItem(const SkPicture*);
  virtual void appendClipItem(
      const blink::WebRect& clip_rect,
      const blink::WebVector<SkRRect>& rounded_clip_rects);
  virtual void appendEndClipItem();
  virtual void appendClipPathItem(const SkPath& clip_path,
                                  SkRegion::Op clip_op,
                                  bool antialias);
  virtual void appendEndClipPathItem();
  virtual void appendFloatClipItem(const blink::WebFloatRect& clip_rect);
  virtual void appendEndFloatClipItem();
  virtual void appendTransformItem(const SkMatrix44& matrix);
  virtual void appendEndTransformItem();
  virtual void appendCompositingItem(float opacity,
                                     SkXfermode::Mode,
                                     SkRect* bounds,
                                     SkColorFilter*);
  virtual void appendEndCompositingItem();
  virtual void appendFilterItem(const blink::WebFilterOperations& filters,
                                const blink::WebFloatRect& bounds);
  virtual void appendEndFilterItem();
  virtual void appendScrollItem(const blink::WebSize& scrollOffset,
                                ScrollContainerId);
  virtual void appendEndScrollItem();

 private:
  cc::DisplayItemList* display_item_list_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayItemListImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
