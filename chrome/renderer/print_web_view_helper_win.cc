// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "chrome/common/print_messages.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/units.h"
#include "skia/ext/vector_canvas.h"
#include "skia/ext/vector_platform_device_emf_win.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using printing::Metafile;
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

void PrintWebViewHelper::PrintPageInternal(
    const PrintMsg_PrintPage_Params& params,
    const gfx::Size& canvas_size,
    WebFrame* frame) {
  // Generate a memory-based metafile. It will use the current screen's DPI.
  // Each metafile contains a single page.
  scoped_ptr<Metafile> metafile(new printing::NativeMetafile);
  metafile->Init();
  DCHECK(metafile->context());
  skia::PlatformDevice::InitializeDC(metafile->context());

  int page_number = params.page_number;

  // Calculate the dpi adjustment.
  float scale_factor = static_cast<float>(params.params.desired_dpi /
                                          params.params.dpi);

  // Render page for printing.
  RenderPage(params.params, &scale_factor, page_number, false, frame,
             &metafile);

  // Close the device context to retrieve the compiled metafile.
  if (!metafile->FinishDocument())
    NOTREACHED();

  // Get the size of the compiled metafile.
  uint32 buf_size = metafile->GetDataSize();
  DCHECK_GT(buf_size, 128u);

  PrintHostMsg_DidPrintPage_Params page_params;
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
  Send(new PrintHostMsg_DuplicateSection(routing_id(),
                                         page_params.metafile_data_handle,
                                         &page_params.metafile_data_handle));
  Send(new PrintHostMsg_DidPrintPage(routing_id(), page_params));
}

bool PrintWebViewHelper::CreatePreviewDocument(
    const PrintMsg_PrintPages_Params& params, WebKit::WebFrame* frame,
    WebKit::WebNode* node) {
  int page_count = 0;
  PrintMsg_Print_Params print_params = params.params;
  UpdatePrintableSizeInPrintParameters(frame, node, &print_params);
  PrepareFrameAndViewForPrint prep_frame_view(print_params, frame, node,
                                              frame->view());
  page_count = prep_frame_view.GetExpectedPageCount();
  if (!page_count)
    return false;

  scoped_ptr<Metafile> metafile(new printing::PreviewMetafile);
  metafile->Init();

  // Calculate the dpi adjustment.
  float shrink = static_cast<float>(print_params.desired_dpi /
                                    print_params.dpi);

  // Record the begin time.
  base::TimeTicks begin_time = base::TimeTicks::Now();

  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      float scale_factor = shrink;
      RenderPage(print_params, &scale_factor, i, true, frame, &metafile);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      if (params.pages[i] >= page_count)
        break;
      float scale_factor = shrink;
      RenderPage(print_params, &scale_factor,
          static_cast<int>(params.pages[i]), true, frame, &metafile);
    }
  }

  // Calculate the time taken to render the requested page for preview and add
  // the net time in the histogram.
  UMA_HISTOGRAM_TIMES("PrintPreview.RenderTime",
                      base::TimeTicks::Now() - begin_time);

  if (!metafile->FinishDocument())
    NOTREACHED();

  // Get the size of the compiled metafile.
  uint32 buf_size = metafile->GetDataSize();
  DCHECK_GT(buf_size, 128u);

  PrintHostMsg_DidPreviewDocument_Params preview_params;
  preview_params.data_size = buf_size;
  preview_params.document_cookie = params.params.document_cookie;
  preview_params.expected_pages_count = page_count;
  preview_params.modifiable = IsModifiable(frame, node);

  if (!CopyMetafileDataToSharedMem(metafile.get(),
                                   &(preview_params.metafile_data_handle))) {
    return false;
  }
  Send(new PrintHostMsg_DuplicateSection(routing_id(),
                                         preview_params.metafile_data_handle,
                                         &preview_params.metafile_data_handle));
  Send(new PrintHostMsg_PagesReadyForPreview(routing_id(), preview_params));
  return true;
}

void PrintWebViewHelper::RenderPage(
    const PrintMsg_Print_Params& params, float* scale_factor, int page_number,
    bool is_preview, WebFrame* frame, scoped_ptr<Metafile>* metafile) {
  double content_width_in_points;
  double content_height_in_points;
  double margin_top_in_points;
  double margin_left_in_points;
  GetPageSizeAndMarginsInPoints(frame, page_number, params,
                                &content_width_in_points,
                                &content_height_in_points,
                                &margin_top_in_points, NULL, NULL,
                                &margin_left_in_points);

  int width;
  int height;
  if (is_preview) {
    int dpi = static_cast<int>(params.dpi);
    int desired_dpi = printing::kPointsPerInch;
    width = ConvertUnit(params.page_size.width(), dpi, desired_dpi);
    height = ConvertUnit(params.page_size.height(), dpi, desired_dpi);
  } else {
    // Since WebKit extends the page width depending on the magical scale factor
    // we make sure the canvas covers the worst case scenario (x2.0 currently).
    // PrintContext will then set the correct clipping region.
    width = static_cast<int>(content_width_in_points * params.max_shrink);
    height = static_cast<int>(content_height_in_points * params.max_shrink);
  }

  gfx::Size page_size(width, height);
  gfx::Rect content_area(static_cast<int>(margin_left_in_points),
                         static_cast<int>(margin_top_in_points),
                         static_cast<int>(content_width_in_points),
                         static_cast<int>(content_height_in_points));
  skia::PlatformDevice* device = (*metafile)->StartPageForVectorCanvas(
      page_size, content_area, frame->getPrintPageShrink(page_number));
  DCHECK(device);
  skia::VectorCanvas canvas(device);

  float webkit_scale_factor = frame->printPage(page_number, &canvas);
  if (*scale_factor <= 0 || webkit_scale_factor <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page |scale_factor|" calculated in
    // webkit.
    *scale_factor /= webkit_scale_factor;
  }

  bool result = (*metafile)->FinishPage();
  DCHECK(result);

  if (!params.supports_alpha_blend) {
    // PreviewMetafile (PDF) supports alpha blend, so we only hit this case
    // for NativeMetafile.
    DCHECK(!is_preview);
    skia::VectorPlatformDeviceEmf* platform_device =
        static_cast<skia::VectorPlatformDeviceEmf*>(device);
    if (platform_device->alpha_blend_used()) {
      // Currently, we handle alpha blend transparency for a single page.
      // Therefore, expecting a metafile with page count 1.
      DCHECK_EQ(1U, (*metafile)->GetPageCount());

      // Close the device context to retrieve the compiled metafile.
      if (!(*metafile)->FinishDocument())
        NOTREACHED();

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

      scoped_ptr<Metafile> metafile2(new printing::NativeMetafile);
      metafile2->Init();
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
}

bool PrintWebViewHelper::CopyMetafileDataToSharedMem(
    Metafile* metafile, base::SharedMemoryHandle* shared_mem_handle) {
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
