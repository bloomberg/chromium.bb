// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_WIDGET_TEST_H_
#define CONTENT_PUBLIC_TEST_RENDER_WIDGET_TEST_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "content/public/test/render_view_test.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace content {

class RenderWidgetTest : public RenderViewTest {
 public:
  RenderWidgetTest();

 protected:
  static const int kNumBytesPerPixel;
  static const int kLargeWidth;
  static const int kLargeHeight;
  static const int kSmallWidth;
  static const int kSmallHeight;
  static const int kTextPositionX;
  static const int kTextPositionY;
  static const uint32 kRedARGB;

  // Helper function which calls OnMsgPaintAtSize and also paints the result
  // in the given bitmap.  The widget is resized to |page_size| before we paint
  // and the final image is resized to |desired_size|. This method is virtual so
  // that TestResizeAndPaint() can be reused by subclasses of this test class.
  virtual void ResizeAndPaint(const gfx::Size& page_size,
                              const gfx::Size& desired_size,
                              SkBitmap* snapshot);

  // Test for ResizeAndPaint.
  void TestResizeAndPaint();

  // Helper function which returns true if the given bitmap contains the given
  // ARGB color and false otherwise.
  bool ImageContainsColor(const SkBitmap& bitmap, uint32 argb_color);

  // This can be used for debugging if you want to output a bitmap
  // image to a file.
  // FilePath tmp_path;
  // file_util::CreateTemporaryFile(&tmp_path);
  // OutputBitmapToFile(bitmap, tmp_path);
  // LOG(INFO) << "Bitmap image stored at: " << tmp_path.value();
  void OutputBitmapToFile(const SkBitmap& bitmap,
                          const base::FilePath& file_path);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_WIDGET_TEST_H_
