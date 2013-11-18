// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_graphics_2d_host.h"

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/pepper/mock_renderer_ppapi_host.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

using blink::WebCanvas;

namespace content {

class PepperGraphics2DHostTest : public testing::Test {
 public:
  static bool ConvertToLogicalPixels(float scale,
                                     gfx::Rect* op_rect,
                                     gfx::Point* delta) {
    return PepperGraphics2DHost::ConvertToLogicalPixels(scale, op_rect, delta);
  }

  PepperGraphics2DHostTest()
      : message_loop_(base::MessageLoop::TYPE_DEFAULT),
        renderer_ppapi_host_(NULL, 12345) {}

  virtual ~PepperGraphics2DHostTest() {
    ppapi::ProxyAutoLock proxy_lock;
    host_.reset();
  }

  void Init(PP_Instance instance,
            const PP_Size& backing_store_size,
            const gfx::Rect& plugin_rect) {
    test_globals_.GetResourceTracker()->DidCreateInstance(instance);
    scoped_refptr<PPB_ImageData_Impl> backing_store(
        new PPB_ImageData_Impl(instance, PPB_ImageData_Impl::ForTest()));
    host_.reset(PepperGraphics2DHost::Create(
        &renderer_ppapi_host_, instance, 12345, backing_store_size, PP_FALSE,
        backing_store));
    DCHECK(host_.get());
    plugin_rect_ = plugin_rect;
  }

  void PaintImageData(PPB_ImageData_Impl* image_data) {
    ppapi::HostResource image_data_resource;
    image_data_resource.SetHostResource(image_data->pp_instance(),
                                        image_data->pp_resource());
    host_->OnHostMsgPaintImageData(NULL, image_data_resource,
                                   PP_Point(), false, PP_Rect());
  }

  void SetOffset(const PP_Point& point) {
    host_->OnHostMsgSetOffset(NULL, point);
  }

  void Flush() {
    ppapi::host::HostMessageContext context(
        ppapi::proxy::ResourceMessageCallParams(host_->pp_resource(), 0));
    host_->OnHostMsgFlush(&context);
    host_->ViewFlushedPaint();
    host_->SendOffscreenFlushAck();
  }

  void PaintToWebCanvas(WebCanvas* canvas) {
    host_->Paint(canvas, plugin_rect_,
                 gfx::Rect(0, 0, plugin_rect_.width(), plugin_rect_.height()));
  }

 private:
  gfx::Rect plugin_rect_;
  scoped_ptr<PepperGraphics2DHost> host_;
  base::MessageLoop message_loop_;
  MockRendererPpapiHost renderer_ppapi_host_;
  ppapi::TestGlobals test_globals_;
};

TEST_F(PepperGraphics2DHostTest, ConvertToLogicalPixels) {
  static const struct {
    int x1;
    int y1;
    int w1;
    int h1;
    int x2;
    int y2;
    int w2;
    int h2;
    int dx1;
    int dy1;
    int dx2;
    int dy2;
    float scale;
    bool result;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, true },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0, true },
    { 0, 0, 4, 4, 0, 0, 2, 2, 0, 0, 0, 0, 0.5, true },
    { 1, 1, 4, 4, 0, 0, 3, 3, 0, 0, 0, 0, 0.5, false },
    { 53, 75, 100, 100, 53, 75, 100, 100, 0, 0, 0, 0, 1.0, true },
    { 53, 75, 100, 100, 106, 150, 200, 200, 0, 0, 0, 0, 2.0, true },
    { 53, 75, 100, 100, 26, 37, 51, 51, 0, 0, 0, 0, 0.5, false },
    { 53, 74, 100, 100, 26, 37, 51, 50, 0, 0, 0, 0, 0.5, false },
    { -1, -1, 100, 100, -1, -1, 51, 51, 0, 0, 0, 0, 0.5, false },
    { -2, -2, 100, 100, -1, -1, 50, 50, 4, -4, 2, -2, 0.5, true },
    { -101, -100, 50, 50, -51, -50, 26, 25, 0, 0, 0, 0, 0.5, false },
    { 10, 10, 20, 20, 5, 5, 10, 10, 0, 0, 0, 0, 0.5, true },
      // Cannot scroll due to partial coverage on sides
    { 11, 10, 20, 20, 5, 5, 11, 10, 0, 0, 0, 0, 0.5, false },
      // Can scroll since backing store is actually smaller/scaling up
    { 11, 20, 100, 100, 22, 40, 200, 200, 7, 3, 14, 6, 2.0, true },
      // Can scroll due to delta and bounds being aligned
    { 10, 10, 20, 20, 5, 5, 10, 10, 6, 4, 3, 2, 0.5, true },
      // Cannot scroll due to dx
    { 10, 10, 20, 20, 5, 5, 10, 10, 5, 4, 2, 2, 0.5, false },
      // Cannot scroll due to dy
    { 10, 10, 20, 20, 5, 5, 10, 10, 6, 3, 3, 1, 0.5, false },
      // Cannot scroll due to top
    { 10, 11, 20, 20, 5, 5, 10, 11, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to left
    { 7, 10, 20, 20, 3, 5, 11, 10, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to width
    { 10, 10, 21, 20, 5, 5, 11, 10, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to height
    { 10, 10, 20, 51, 5, 5, 10, 26, 6, 4, 3, 2, 0.5, false },
      // Check negative scroll deltas
    { 10, 10, 20, 20, 5, 5, 10, 10, -6, -4, -3, -2, 0.5, true },
    { 10, 10, 20, 20, 5, 5, 10, 10, -6, -3, -3, -1, 0.5, false },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect orig = r1;
    gfx::Point delta(tests[i].dx1, tests[i].dy1);
    bool res = ConvertToLogicalPixels(tests[i].scale, &r1, &delta);
    EXPECT_EQ(r2.ToString(), r1.ToString());
    EXPECT_EQ(res, tests[i].result);
    if (res) {
      EXPECT_EQ(delta, gfx::Point(tests[i].dx2, tests[i].dy2));
    }
    // Reverse the scale and ensure all the original pixels are still inside
    // the result.
    ConvertToLogicalPixels(1.0f / tests[i].scale, &r1, NULL);
    EXPECT_TRUE(r1.Contains(orig));
  }
}

TEST_F(PepperGraphics2DHostTest, SetOffset) {
  ppapi::ProxyAutoLock proxy_lock;

  // Initialize the backing store.
  PP_Instance instance = 12345;
  PP_Size backing_store_size = { 300, 300 };
  gfx::Rect plugin_rect(0, 0, 500, 500);
  Init(instance, backing_store_size, plugin_rect);

  // Paint the entire backing store red.
  scoped_refptr<PPB_ImageData_Impl> image_data(
      new PPB_ImageData_Impl(instance, PPB_ImageData_Impl::ForTest()));
  image_data->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                   backing_store_size.width,
                   backing_store_size.height,
                   true);
  {
    ImageDataAutoMapper auto_mapper(image_data.get());
    image_data->GetMappedBitmap()->eraseColor(
        SkColorSetARGBMacro(255, 255, 0, 0));
  }
  PaintImageData(image_data.get());
  Flush();

  // Set up the actual and expected bitmaps/canvas.
  SkBitmap actual_bitmap;
  actual_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                          plugin_rect.x() + plugin_rect.width(),
                          plugin_rect.y() + plugin_rect.height());
  actual_bitmap.allocPixels();
  actual_bitmap.eraseColor(0);
  WebCanvas actual_canvas(actual_bitmap);
  SkBitmap expected_bitmap;
  expected_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                            plugin_rect.x() + plugin_rect.width(),
                            plugin_rect.y() + plugin_rect.height());
  expected_bitmap.allocPixels();
  expected_bitmap.eraseColor(0);

  // Paint the backing store to the canvas.
  PaintToWebCanvas(&actual_canvas);
  expected_bitmap.eraseArea(
      SkIRect::MakeWH(backing_store_size.width, backing_store_size.height),
      SkColorSetARGBMacro(255, 255, 0, 0));
  EXPECT_EQ(memcmp(expected_bitmap.getAddr(0, 0),
                   actual_bitmap.getAddr(0, 0),
                   expected_bitmap.getSize()), 0);

  // Set the offset.
  PP_Point offset = { 20, 20 };
  SetOffset(offset);
  actual_bitmap.eraseColor(0);
  PaintToWebCanvas(&actual_canvas);
  // No flush has occurred so the result should be the same.
  EXPECT_EQ(memcmp(expected_bitmap.getAddr(0, 0),
                   actual_bitmap.getAddr(0, 0),
                   expected_bitmap.getSize()), 0);


  // Flush the offset and the location of the rectangle should have shifted.
  Flush();
  actual_bitmap.eraseColor(0);
  PaintToWebCanvas(&actual_canvas);
  expected_bitmap.eraseColor(0);
  expected_bitmap.eraseArea(
      SkIRect::MakeXYWH(offset.x, offset.y,
                        backing_store_size.width, backing_store_size.height),
      SkColorSetARGBMacro(255, 255, 0, 0));
  EXPECT_EQ(memcmp(expected_bitmap.getAddr(0, 0),
                   actual_bitmap.getAddr(0, 0),
                   expected_bitmap.getSize()), 0);
}

}  // namespace content
