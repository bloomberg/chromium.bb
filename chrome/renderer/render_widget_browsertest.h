// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_WIDGET_BROWSERTEST_H_
#define CHROME_RENDERER_RENDER_WIDGET_BROWSERTEST_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/test/render_view_test.h"

namespace gfx {
class Size;
}

class SkBitmap;
class TransportDIB;

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
  static const int kSequenceNum;
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
  void OutputBitmapToFile(const SkBitmap& bitmap, const FilePath& file_path);
};

#endif  // CHROME_RENDERER_RENDER_WIDGET_BROWSERTEST_H_
