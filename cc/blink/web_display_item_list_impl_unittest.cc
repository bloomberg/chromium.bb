// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_display_item_list_impl.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"

namespace cc_blink {
namespace {

// SkCanvas::saveLayer(Alpha) allows for an optional bounds which is a hint
// for the backing size to use for the indirect buffer.  It is not really
// a clip though, as Skia is allowed to use any size buffer it wants.
// Additionally, PaintOpBuffer can drop the bounds when folding
// save/draw/restore into a single draw-with-alpha.  However, in some cases
// calling code (SVGMask) is drawing unclipped items which really need
// a clip to not pollute the rest of the painted content with their draw
// commands.  Therefore, this tests that when providing bounds to
// AppendCompositingItem that a clip is inserted.
TEST(WebDisplayItemListImpl, ClipWhenCompositing) {
  // Test with two different blend modes to differentiate the saveLayer vs
  // saveLayerAlpha logic inside of AppendCompositingItem;
  SkBlendMode blend_modes[] = {SkBlendMode::kSrc, SkBlendMode::kSrcOver};
  for (size_t i = 0; i < arraysize(blend_modes); ++i) {
    static constexpr int size = 20;
    static constexpr SkColor background_color = SK_ColorMAGENTA;
    static constexpr SkColor clip_color = SK_ColorYELLOW;

    blink::WebRect full_bounds(0, 0, size, size);
    blink::WebRect clip_bounds(5, 3, 13, 9);
    SkRect sk_clip_bounds = SkRect::MakeXYWH(
        clip_bounds.x, clip_bounds.y, clip_bounds.width, clip_bounds.height);
    SkIRect clip_irect = sk_clip_bounds.roundOut();

    auto cc_list = base::MakeRefCounted<cc::DisplayItemList>();
    cc_blink::WebDisplayItemListImpl web_list(cc_list.get());

    // drawColor(background color)
    // saveLayer(should clip)
    //   drawColor(clip color)
    // end
    auto background_record = sk_make_sp<cc::PaintRecord>();
    background_record->push<cc::DrawColorOp>(background_color,
                                             SkBlendMode::kSrcOver);
    web_list.AppendDrawingItem(full_bounds, background_record);
    web_list.AppendCompositingItem(1.f, blend_modes[i], &sk_clip_bounds,
                                   nullptr);
    auto clip_record = sk_make_sp<cc::PaintRecord>();
    clip_record->push<cc::DrawColorOp>(clip_color, SkBlendMode::kSrcOver);
    web_list.AppendDrawingItem(full_bounds, clip_record);
    web_list.AppendEndCompositingItem();
    cc_list->Finalize();

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
    SkCanvas canvas(bitmap);
    canvas.drawColor(SK_ColorGRAY);

    cc_list->Raster(&canvas);
    uint8_t* pixels = static_cast<uint8_t*>(bitmap.getPixels());
    size_t row_bytes = size * 4;
    for (size_t y = 0; y < size; ++y) {
      size_t y_offset = y * row_bytes;
      for (size_t x = 0; x < size; ++x) {
        size_t x_offset = x * 4;
        uint8_t* pixel = &pixels[y_offset + x_offset];
        SCOPED_TRACE(
            base::StringPrintf("x(%zd) y(%zd) mode(%d)", x, y, blend_modes[i]));

        if (clip_irect.contains(x, y)) {
          EXPECT_EQ(SkColorGetR(clip_color), pixel[SK_R32_SHIFT / 8]);
          EXPECT_EQ(SkColorGetG(clip_color), pixel[SK_G32_SHIFT / 8]);
          EXPECT_EQ(SkColorGetB(clip_color), pixel[SK_B32_SHIFT / 8]);
          EXPECT_EQ(SkColorGetA(clip_color), pixel[SK_A32_SHIFT / 8]);
        } else {
          EXPECT_EQ(SkColorGetR(background_color), pixel[SK_R32_SHIFT / 8]);
          EXPECT_EQ(SkColorGetG(background_color), pixel[SK_G32_SHIFT / 8]);
          EXPECT_EQ(SkColorGetB(background_color), pixel[SK_B32_SHIFT / 8]);
          EXPECT_EQ(SkColorGetA(background_color), pixel[SK_A32_SHIFT / 8]);
        }
      }
    }
  }
}

}  // namespace
}  // namespace cc_blink
