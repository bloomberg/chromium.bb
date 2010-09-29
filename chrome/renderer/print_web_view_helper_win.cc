// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "gfx/size.h"
#include "grit/generated_resources.h"
#include "printing/native_metafile.h"
#include "printing/units.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using WebKit::WebFrame;
using WebKit::WebString;

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  // Generate a memory-based metafile. It will use the current screen's DPI.
  printing::NativeMetafile metafile;

  metafile.CreateDc(NULL, NULL);
  HDC hdc = metafile.hdc();
  DCHECK(hdc);
  skia::PlatformDevice::InitializeDC(hdc);

  double content_width_in_points;
  double content_height_in_points;
  double margin_top_in_points;
  double margin_right_in_points;
  double margin_bottom_in_points;
  double margin_left_in_points;
  GetPageSizeAndMarginsInPoints(frame,
                                params.page_number,
                                params.params,
                                &content_width_in_points,
                                &content_height_in_points,
                                &margin_top_in_points,
                                &margin_right_in_points,
                                &margin_bottom_in_points,
                                &margin_left_in_points);

  // Since WebKit extends the page width depending on the magical shrink
  // factor we make sure the canvas covers the worst case scenario
  // (x2.0 currently).  PrintContext will then set the correct clipping region.
  int size_x = static_cast<int>(content_width_in_points *
                                params.params.max_shrink);
  int size_y = static_cast<int>(content_height_in_points *
                                params.params.max_shrink);
  // Calculate the dpi adjustment.
  float shrink = static_cast<float>(params.params.desired_dpi /
                                    params.params.dpi);
#if 0
  // TODO(maruel): This code is kept for testing until the 100% GDI drawing
  // code is stable. maruels use this code's output as a reference when the
  // GDI drawing code fails.

  // Mix of Skia and GDI based.
  skia::PlatformCanvas canvas(size_x, size_y, true);
  canvas.drawARGB(255, 255, 255, 255, SkPorterDuff::kSrc_Mode);
  float webkit_shrink = frame->PrintPage(params.page_number, &canvas);
  if (shrink <= 0 || webkit_shrink <= 0) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page shrink" calculated in webkit.
    shrink /= webkit_shrink;
  }

  // Create a BMP v4 header that we can serialize.
  BITMAPV4HEADER bitmap_header;
  gfx::CreateBitmapV4Header(size_x, size_y, &bitmap_header);
  const SkBitmap& src_bmp = canvas.getDevice()->accessBitmap(true);
  SkAutoLockPixels src_lock(src_bmp);
  int retval = StretchDIBits(hdc,
                             0,
                             0,
                             size_x, size_y,
                             0, 0,
                             size_x, size_y,
                             src_bmp.getPixels(),
                             reinterpret_cast<BITMAPINFO*>(&bitmap_header),
                             DIB_RGB_COLORS,
                             SRCCOPY);
  DCHECK(retval != GDI_ERROR);
#else
  // 100% GDI based.
  skia::VectorCanvas canvas(hdc, size_x, size_y);
  float webkit_shrink = frame->printPage(params.page_number, &canvas);
  if (shrink <= 0 || webkit_shrink <= 0) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page shrink" calculated in webkit.
    shrink /= webkit_shrink;
  }
#endif

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile.CloseDc()) {
    NOTREACHED() << "metafile failed";
  }

  // Get the size of the compiled metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 128u);
  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = 0;
  page_params.metafile_data_handle = NULL;
  page_params.page_number = params.page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = shrink;
  page_params.page_size = gfx::Size(
      static_cast<int>(ConvertUnitDouble(
          content_width_in_points +
          margin_left_in_points + margin_right_in_points,
          kPointsPerInch, params.params.dpi)),
      static_cast<int>(ConvertUnitDouble(
          content_height_in_points +
          margin_top_in_points + margin_bottom_in_points,
          kPointsPerInch, params.params.dpi)));
  page_params.content_area = gfx::Rect(
      static_cast<int>(ConvertUnitDouble(
          margin_left_in_points, kPointsPerInch, params.params.dpi)),
      static_cast<int>(ConvertUnitDouble(
          margin_top_in_points, kPointsPerInch, params.params.dpi)),
      static_cast<int>(ConvertUnitDouble(
          content_width_in_points, kPointsPerInch, params.params.dpi)),
      static_cast<int>(ConvertUnitDouble(
          content_height_in_points, kPointsPerInch, params.params.dpi)));
  page_params.has_visible_overlays =
      frame->isPageBoxVisible(params.page_number);
  base::SharedMemory shared_buf;

  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (buf_size < 350*1024*1024) {
    // Allocate a shared memory buffer to hold the generated metafile data.
    if (shared_buf.Create("", false, false, buf_size) &&
        shared_buf.Map(buf_size)) {
      // Copy the bits into shared memory.
      if (metafile.GetData(shared_buf.memory(), buf_size)) {
        page_params.metafile_data_handle = shared_buf.handle();
        page_params.data_size = buf_size;
      } else {
        NOTREACHED() << "GetData() failed";
      }
      shared_buf.Unmap();
    } else {
      NOTREACHED() << "Buffer allocation failed";
    }
  } else {
    NOTREACHED() << "Buffer too large: " << buf_size;
  }
  metafile.CloseEmf();
  if (Send(new ViewHostMsg_DuplicateSection(
      routing_id(),
      page_params.metafile_data_handle,
      &page_params.metafile_data_handle))) {
    Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
  }
}
