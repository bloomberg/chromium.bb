// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "gfx/gdi_util.h"
#include "gfx/size.h"
#include "grit/generated_resources.h"
#include "printing/native_metafile.h"
#include "printing/units.h"
#include "skia/ext/vector_canvas.h"
#include "skia/ext/vector_platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using WebKit::WebFrame;
using WebKit::WebString;

int CALLBACK EnhMetaFileProc(HDC dc,
                             HANDLETABLE* handle_table,
                             const ENHMETARECORD *record,
                             int num_objects,
                             LPARAM data) {
  HDC* bitmap_dc = reinterpret_cast<HDC*>(data);
  // Play this command to the bitmap DC.
  PlayEnhMetaFileRecord(*bitmap_dc, handle_table, record, num_objects);
  if (record->iType == EMR_ALPHABLEND) {
    const EMRALPHABLEND* emr_alpha_blend =
        reinterpret_cast<const EMRALPHABLEND*>(record);
    XFORM bitmap_dc_transform, metafile_dc_transform;
    XFORM identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    // Temporarily set the world transforms of both DC's to identity.
    GetWorldTransform(dc, &metafile_dc_transform);
    SetWorldTransform(dc, &identity);
    GetWorldTransform(*bitmap_dc, &bitmap_dc_transform);
    SetWorldTransform(*bitmap_dc, &identity);
    const RECTL& rect = emr_alpha_blend->rclBounds;
    // Since the printer does not support alpha blend, copy the alpha blended
    // region from our (software-rendered) bitmap DC to the metafile DC.
    BitBlt(dc,
           rect.left,
           rect.top,
           rect.right - rect.left + 1,
           rect.bottom - rect.top + 1,
           *bitmap_dc,
           rect.left,
           rect.top,
           SRCCOPY);
    // Restore the world transforms of both DC's.
    SetWorldTransform(dc, &metafile_dc_transform);
    SetWorldTransform(*bitmap_dc, &bitmap_dc_transform);
  } else {
    // Play this command to the metafile DC.
    PlayEnhMetaFileRecord(dc, handle_table, record, num_objects);
  }
  return 1;  // Continue enumeration
}

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  // Generate a memory-based metafile. It will use the current screen's DPI.
  printing::NativeMetafile metafile;

  metafile.CreateDc(NULL, NULL);
  HDC hdc = metafile.hdc();
  DCHECK(hdc);
  skia::PlatformDevice::InitializeDC(hdc);

  int page_number = params.page_number;

  double content_width_in_points;
  double content_height_in_points;
  GetPageSizeAndMarginsInPoints(frame, page_number, params.params,
      &content_width_in_points, &content_height_in_points, NULL, NULL, NULL,
      NULL);

  // Calculate the dpi adjustment.
  float scale_factor = static_cast<float>(params.params.desired_dpi /
                                          params.params.dpi);

  // Since WebKit extends the page width depending on the magical |scale_factor|
  // we make sure the canvas covers the worst case scenario (x2.0 currently).
  // PrintContext will then set the correct clipping region.
  int width = static_cast<int>(content_width_in_points *
                               params.params.max_shrink);
  int height = static_cast<int>(content_height_in_points *
                                params.params.max_shrink);
#if 0
  // TODO(maruel): This code is kept for testing until the 100% GDI drawing
  // code is stable. maruels use this code's output as a reference when the
  // GDI drawing code fails.

  // Mix of Skia and GDI based.
  skia::PlatformCanvas canvas(width, height, true);
  canvas.drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
  float webkit_scale_factor = frame->printPage(page_number, &canvas);
  if (scale_factor <= 0 || webkit_scale_factor <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page |scale_factor|" calculated
    // in webkit.
    scale_factor /= webkit_scale_factor;
  }

  // Create a BMP v4 header that we can serialize.
  BITMAPV4HEADER bitmap_header;
  gfx::CreateBitmapV4Header(width, height, &bitmap_header);
  const SkBitmap& src_bmp = canvas.getDevice()->accessBitmap(true);
  SkAutoLockPixels src_lock(src_bmp);
  int retval = StretchDIBits(hdc,
                             0,
                             0,
                             width, height,
                             0, 0,
                             width, height,
                             src_bmp.getPixels(),
                             reinterpret_cast<BITMAPINFO*>(&bitmap_header),
                             DIB_RGB_COLORS,
                             SRCCOPY);
  DCHECK(retval != GDI_ERROR);
#else
  // 100% GDI based.
  skia::VectorCanvas canvas(hdc, width, height);
  float webkit_scale_factor = frame->printPage(page_number, &canvas);
  if (scale_factor <= 0 || webkit_scale_factor <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page scale_factor" calculated
    // in webkit.
    scale_factor /= webkit_scale_factor;
  }
#endif

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile.CloseDc()) {
    NOTREACHED() << "metafile failed";
  }
  printing::NativeMetafile* mf = &metafile;
  printing::NativeMetafile metafile2;

  skia::VectorPlatformDevice* platform_device =
    static_cast<skia::VectorPlatformDevice*>(canvas.getDevice());
  if (platform_device->alpha_blend_used() &&
      !params.params.supports_alpha_blend) {
    // Page used alpha blend, but printer doesn't support it.  Rewrite the
    // metafile and flatten out the transparency.
    HDC bitmap_dc = CreateCompatibleDC(GetDC(NULL));
    if (!bitmap_dc) {
      NOTREACHED() << "Bitmap DC creation failed";
    }
    SetGraphicsMode(bitmap_dc, GM_ADVANCED);
    void* bits = NULL;
    BITMAPINFO hdr;
    gfx::CreateBitmapHeader(width, height, &hdr.bmiHeader);
    HBITMAP hbitmap = CreateDIBSection(
        bitmap_dc, &hdr, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbitmap) {
      NOTREACHED() << "Raster bitmap creation for printing failed";
    }

    HGDIOBJ old_bitmap = SelectObject(bitmap_dc, hbitmap);
    RECT rect = {0, 0, width, height };
    HBRUSH whiteBrush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    FillRect(bitmap_dc, &rect, whiteBrush);

    metafile2.CreateDc(NULL, NULL);
    HDC hdc = metafile2.hdc();
    DCHECK(hdc);
    skia::PlatformDevice::InitializeDC(hdc);

    RECT metafile_bounds = metafile.GetBounds().ToRECT();
    // Process the old metafile, placing all non-AlphaBlend calls into the
    // new metafile, and copying the results of all the AlphaBlend calls
    // from the bitmap DC.
    EnumEnhMetaFile(hdc,
                    metafile.emf(),
                    EnhMetaFileProc,
                    &bitmap_dc,
                    &metafile_bounds);

    SelectObject(bitmap_dc, old_bitmap);

    if (!metafile2.CloseDc()) {
      NOTREACHED() << "metafile failed";
    }
    mf = &metafile2;
  }

  // Get the size of the compiled metafile.
  uint32 buf_size = mf->GetDataSize();
  DCHECK_GT(buf_size, 128u);
  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = 0;
  page_params.metafile_data_handle = NULL;
  page_params.page_number = page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = scale_factor;
  page_params.page_size = params.params.page_size;
  page_params.content_area = gfx::Rect(params.params.margin_left,
      params.params.margin_top, params.params.printable_size.width(),
      params.params.printable_size.height());
  page_params.has_visible_overlays = frame->isPageBoxVisible(page_number);
  base::SharedMemory shared_buf;

  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (buf_size < 350*1024*1024) {
    // Allocate a shared memory buffer to hold the generated metafile data.
    if (shared_buf.CreateAndMapAnonymous(buf_size)) {
      // Copy the bits into shared memory.
      if (mf->GetData(shared_buf.memory(), buf_size)) {
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
  mf->CloseEmf();
  if (Send(new ViewHostMsg_DuplicateSection(
      routing_id(),
      page_params.metafile_data_handle,
      &page_params.metafile_data_handle))) {
    if (!is_preview_) {
      Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
    }
  }
}
