// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
#define CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_

#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/resources/display_item_list.h"
#include "third_party/WebKit/public/platform/WebBlendMode.h"
#include "third_party/WebKit/public/platform/WebContentLayerClient.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebVector.h"

#if WEB_DISPLAY_ITEM_LIST_IS_DEFINED
#include "third_party/WebKit/public/platform/WebDisplayItemList.h"
#endif

class SkImageFilter;
class SkMatrix44;
class SkPicture;
class SkRRect;

namespace cc_blink {

#if WEB_DISPLAY_ITEM_LIST_IS_DEFINED
class WebDisplayItemListImpl : public blink::WebDisplayItemList {
#else
class WebDisplayItemListImpl {
#endif

 public:
  CC_BLINK_EXPORT WebDisplayItemListImpl();
  virtual ~WebDisplayItemListImpl();

  scoped_refptr<cc::DisplayItemList> ToDisplayItemList();

  // blink::WebDisplayItemList implementation.
  virtual void appendDrawingItem(SkPicture* picture,
                                 const blink::WebFloatPoint& location);
  virtual void appendClipItem(
      const blink::WebRect& clip_rect,
      const blink::WebVector<SkRRect>& rounded_clip_rects);
  virtual void appendEndClipItem();
  virtual void appendFloatClipItem(const blink::WebFloatRect& clip_rect);
  virtual void appendEndFloatClipItem();
  virtual void appendTransformItem(const SkMatrix44& matrix);
  virtual void appendEndTransformItem();
  virtual void appendTransparencyItem(float opacity,
                                      blink::WebBlendMode blend_mode);
  virtual void appendEndTransparencyItem();
  virtual void appendFilterItem(SkImageFilter* filter,
                                const blink::WebFloatRect& bounds);
  virtual void appendEndFilterItem();

 private:
  scoped_refptr<cc::DisplayItemList> display_item_list_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayItemListImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_DISPLAY_ITEM_LIST_IMPL_H_
