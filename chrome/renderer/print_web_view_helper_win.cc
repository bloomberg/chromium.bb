// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "printing/native_metafile_factory.h"
#include "printing/native_metafile.h"
#include "printing/units.h"
#include "skia/ext/vector_canvas.h"
#include "skia/ext/vector_platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "ui/gfx/gdi_util.h"

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using WebKit::WebFrame;

namespace {

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

}  // namespace

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  // Generate a memory-based metafile. It will use the current screen's DPI.
  // Each metafile contains a single page.
  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  metafile->CreateDc(NULL, NULL);
  DCHECK(metafile->context());
  skia::PlatformDevice::InitializeDC(metafile->context());

  int page_number = params.page_number;

  // Calculate the dpi adjustment.
  float scale_factor = static_cast<float>(params.params.desired_dpi /
                                          params.params.dpi);

  // Render page for printing.
  RenderPage(params.params, &scale_factor, page_number, frame, &metafile);

  // Close the device context to retrieve the compiled metafile.
  if (!metafile->Close())
    NOTREACHED();

  // Get the size of the compiled metafile.
  uint32 buf_size = metafile->GetDataSize();
  DCHECK_GT(buf_size, 128u);

  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = buf_size;
  page_params.metafile_data_handle = NULL;
  page_params.page_number = page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = scale_factor;
  page_params.page_size = params.params.page_size;
  page_params.content_area = gfx::Rect(params.params.margin_left,
      params.params.margin_top, params.params.printable_size.width(),
      params.params.printable_size.height());
  page_params.has_visible_overlays = frame->isPageBoxVisible(page_number);

  if (!CopyMetafileDataToSharedMem(metafile.get(),
                                   &(page_params.metafile_data_handle))) {
    page_params.data_size = 0;
  }
  metafile->CloseEmf();
  if (!Send(new ViewHostMsg_DuplicateSection(
          routing_id(),
          page_params.metafile_data_handle,
          &page_params.metafile_data_handle))) {
    NOTREACHED() << "Send message failed.";
  }
  Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
}

void PrintWebViewHelper::CreatePreviewDocument(
    const ViewMsg_PrintPages_Params& params, WebKit::WebFrame* frame,
    WebKit::WebNode* node) {
  int page_count = 0;
  ViewMsg_Print_Params print_params = params.params;
  UpdatePrintableSizeInPrintParameters(frame, node, &print_params);
  PrepareFrameAndViewForPrint prep_frame_view(print_params, frame, node,
                                              frame->view());
  page_count = prep_frame_view.GetExpectedPageCount();
  if (!page_count)
    return;

  // NOTE: This is an enhanced-format metafile(EMF) which has an appearance of
  // single page metafile. For print preview, we need a metafile with multiple
  // pages.
  // TODO(kmadhusu): Use a PDF metafile to support multiple pages. After "Skia
  // PDF backend" work is completed for windows, make changes to replace this
  // EMF with PDF metafile.
  // http://code.google.com/p/chromium/issues/detail?id=62889
  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  metafile->CreateDc(NULL, NULL);
  DCHECK(metafile->context());
  skia::PlatformDevice::InitializeDC(metafile->context());

  // Calculate the dpi adjustment.
  float shrink = static_cast<float>(params.params.desired_dpi /
                                    params.params.dpi);

  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      float scale_factor = shrink;
      RenderPage(params.params, &scale_factor, i, frame, &metafile);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      if (params.pages[i] >= page_count)
        break;
      float scale_factor = shrink;
      RenderPage(params.params, &scale_factor,
          static_cast<int>(params.pages[i]), frame, &metafile);
    }
  }

  // Close the device context to retrieve the compiled metafile.
  if (!metafile->Close())
    NOTREACHED();

  // Get the size of the compiled metafile.
  uint32 buf_size = metafile->GetDataSize();
  DCHECK_GT(buf_size, 128u);

  ViewHostMsg_DidPreviewDocument_Params preview_params;
  preview_params.document_cookie = params.params.document_cookie;
  preview_params.data_size = buf_size;
  preview_params.metafile_data_handle = NULL;
  preview_params.expected_pages_count = page_count;

  if (!CopyMetafileDataToSharedMem(metafile.get(),
                                   &(preview_params.metafile_data_handle))) {
    preview_params.data_size = 0;
    preview_params.expected_pages_count = 0;
  }
  metafile->CloseEmf();
  if (!Send(new ViewHostMsg_DuplicateSection(
          routing_id(),
          preview_params.metafile_data_handle,
          &preview_params.metafile_data_handle))) {
    NOTREACHED() << "Send message failed.";
  }
  Send(new ViewHostMsg_PagesReadyForPreview(routing_id(), preview_params));
}

void PrintWebViewHelper::RenderPage(
    const ViewMsg_Print_Params& params, float* scale_factor, int page_number,
    WebFrame* frame, scoped_ptr<printing::NativeMetafile>* metafile) {
  HDC hdc = (*metafile)->context();
  DCHECK(hdc);

  double content_width_in_points;
  double content_height_in_points;
  GetPageSizeAndMarginsInPoints(frame, page_number, params,
      &content_width_in_points, &content_height_in_points, NULL, NULL, NULL,
      NULL);

  // Since WebKit extends the page width depending on the magical scale factor
  // we make sure the canvas covers the worst case scenario (x2.0 currently).
  // PrintContext will then set the correct clipping region.
  int width = static_cast<int>(content_width_in_points * params.max_shrink);
  int height = static_cast<int>(content_height_in_points * params.max_shrink);

#if 0
  // TODO(maruel): This code is kept for testing until the 100% GDI drawing
  // code is stable. maruels use this code's output as a reference when the
  // GDI drawing code fails.

  // Mix of Skia and GDI based.
  skia::PlatformCanvas canvas(width, height, true);
  canvas.drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
  float webkit_scale_factor = frame->printPage(page_number, &canvas);
  if (*scale_factor <= 0 || webkit_scale_factor <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page |scale_factor|" calculated in
    // webkit.
    *scale_factor /= webkit_scale_factor;
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
  if (*scale_factor <= 0 || webkit_scale_factor <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page |scale_factor|" calculated in
    // webkit.
    *scale_factor /= webkit_scale_factor;
  }
#endif

  skia::VectorPlatformDevice* platform_device =
      static_cast<skia::VectorPlatformDevice*>(canvas.getDevice());
  if (platform_device->alpha_blend_used() && !params.supports_alpha_blend) {
    // Close the device context to retrieve the compiled metafile.
    if (!(*metafile)->Close())
      NOTREACHED();

    scoped_ptr<printing::NativeMetafile> metafile2(
        printing::NativeMetafileFactory::CreateMetafile());
    // Page used alpha blend, but printer doesn't support it.  Rewrite the
    // metafile and flatten out the transparency.
    HDC bitmap_dc = CreateCompatibleDC(GetDC(NULL));
    if (!bitmap_dc)
      NOTREACHED() << "Bitmap DC creation failed";
    SetGraphicsMode(bitmap_dc, GM_ADVANCED);
    void* bits = NULL;
    BITMAPINFO hdr;
    gfx::CreateBitmapHeader(width, height, &hdr.bmiHeader);
    HBITMAP hbitmap = CreateDIBSection(
        bitmap_dc, &hdr, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbitmap)
      NOTREACHED() << "Raster bitmap creation for printing failed";

    HGDIOBJ old_bitmap = SelectObject(bitmap_dc, hbitmap);
    RECT rect = {0, 0, width, height };
    HBRUSH whiteBrush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    FillRect(bitmap_dc, &rect, whiteBrush);

    metafile2->CreateDc(NULL, NULL);
    HDC hdc = metafile2->context();
    DCHECK(hdc);
    skia::PlatformDevice::InitializeDC(hdc);

    RECT metafile_bounds = (*metafile)->GetPageBounds(1).ToRECT();
    // Process the old metafile, placing all non-AlphaBlend calls into the
    // new metafile, and copying the results of all the AlphaBlend calls
    // from the bitmap DC.
    EnumEnhMetaFile(hdc,
                    (*metafile)->emf(),
                    EnhMetaFileProc,
                    &bitmap_dc,
                    &metafile_bounds);

    SelectObject(bitmap_dc, old_bitmap);
    metafile->reset(metafile2.release());
  }
}

bool PrintWebViewHelper::CopyMetafileDataToSharedMem(
    printing::NativeMetafile* metafile,
    base::SharedMemoryHandle* shared_mem_handle) {
  uint32 buf_size = metafile->GetDataSize();
  base::SharedMemory shared_buf;

  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (buf_size >= 350*1024*1024) {
    NOTREACHED() << "Buffer too large: " << buf_size;
    return false;
  }

  // Allocate a shared memory buffer to hold the generated metafile data.
  if (!shared_buf.CreateAndMapAnonymous(buf_size)) {
    NOTREACHED() << "Buffer allocation failed";
    return false;
  }

  // Copy the bits into shared memory.
  if (!metafile->GetData(shared_buf.memory(), buf_size)) {
    NOTREACHED() << "GetData() failed";
    shared_buf.Unmap();
    return false;
  }
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), shared_mem_handle);
  shared_buf.Unmap();
  return true;
}
