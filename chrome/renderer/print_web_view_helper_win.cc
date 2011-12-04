// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "chrome/common/print_messages.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/metafile_skia_wrapper.h"
#include "printing/page_size_margins.h"
#include "printing/units.h"
#include "skia/ext/vector_canvas.h"
#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkRefCnt.h"
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
  switch (record->iType) {
    case EMR_ALPHABLEND: {
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
      // Since the printer does not support alpha blend, copy the alpha
      // blended region from our (software-rendered) bitmap DC to the
      // metafile DC.
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
      break;
    }

    case EMR_CREATEBRUSHINDIRECT:
    case EMR_CREATECOLORSPACE:
    case EMR_CREATECOLORSPACEW:
    case EMR_CREATEDIBPATTERNBRUSHPT:
    case EMR_CREATEMONOBRUSH:
    case EMR_CREATEPALETTE:
    case EMR_CREATEPEN:
    case EMR_DELETECOLORSPACE:
    case EMR_DELETEOBJECT:
    case EMR_EXTCREATEFONTINDIRECTW:
      // Play object creation command only once.
      break;

    default:
      // Play this command to the metafile DC.
      PlayEnhMetaFileRecord(dc, handle_table, record, num_objects);
      break;
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
  skia::InitializeDC(metafile->context());

  int page_number = params.page_number;

  // Calculate the dpi adjustment.
  float scale_factor = static_cast<float>(params.params.desired_dpi /
                                          params.params.dpi);

  // Render page for printing.
  metafile.reset(RenderPage(params.params, &scale_factor, page_number, false,
                            frame, metafile.get()));

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
      params.params.margin_top, params.params.content_size.width(),
      params.params.content_size.height());

  if (!CopyMetafileDataToSharedMem(metafile.get(),
                                   &(page_params.metafile_data_handle))) {
    page_params.data_size = 0;
  }

  Send(new PrintHostMsg_DidPrintPage(routing_id(), page_params));
}

bool PrintWebViewHelper::RenderPreviewPage(int page_number) {
  PrintMsg_Print_Params print_params = print_preview_context_.print_params();
  // Calculate the dpi adjustment.
  float scale_factor = static_cast<float>(print_params.desired_dpi /
                                          print_params.dpi);
  scoped_ptr<Metafile> draft_metafile;
  printing::Metafile* initial_render_metafile =
      print_preview_context_.metafile();

  if (print_preview_context_.IsModifiable() && is_print_ready_metafile_sent_) {
    draft_metafile.reset(new printing::PreviewMetafile);
    initial_render_metafile = draft_metafile.get();
  }

  base::TimeTicks begin_time = base::TimeTicks::Now();
  printing::Metafile* render_page_result =
      RenderPage(print_params, &scale_factor, page_number, true,
                 print_preview_context_.frame(), initial_render_metafile);
  // In the preview flow, RenderPage will never return a new metafile.
  DCHECK_EQ(render_page_result, initial_render_metafile);
  print_preview_context_.RenderedPreviewPage(
      base::TimeTicks::Now() - begin_time);

  if (draft_metafile.get()) {
    draft_metafile->FinishDocument();
  } else if (print_preview_context_.IsModifiable() &&
             print_preview_context_.generate_draft_pages()) {
    DCHECK(!draft_metafile.get());
    draft_metafile.reset(
        print_preview_context_.metafile()->GetMetafileForCurrentPage());
  }
  return PreviewPageRendered(page_number, draft_metafile.get());
}

Metafile* PrintWebViewHelper::RenderPage(
    const PrintMsg_Print_Params& params, float* scale_factor, int page_number,
    bool is_preview, WebFrame* frame, Metafile* metafile) {
  printing::PageSizeMargins page_layout_in_points;
  GetPageSizeAndMarginsInPoints(frame, page_number, params,
                                &page_layout_in_points);

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
    width = static_cast<int>(page_layout_in_points.content_width *
                             params.max_shrink);
    height = static_cast<int>(page_layout_in_points.content_height *
                              params.max_shrink);
  }

  gfx::Size page_size(width, height);
  gfx::Rect content_area(
      static_cast<int>(page_layout_in_points.margin_left),
      static_cast<int>(page_layout_in_points.margin_top),
      static_cast<int>(page_layout_in_points.content_width),
      static_cast<int>(page_layout_in_points.content_height));
  SkDevice* device = metafile->StartPageForVectorCanvas(
      page_size, content_area, frame->getPrintPageShrink(page_number));
  DCHECK(device);
  // The printPage method may take a reference to the canvas we pass down, so it
  // can't be a stack object.
  SkRefPtr<skia::VectorCanvas> canvas = new skia::VectorCanvas(device);
  canvas->unref();  // SkRefPtr and new both took a reference.
  if (is_preview) {
    printing::MetafileSkiaWrapper::SetMetafileOnCanvas(*canvas, metafile);
    skia::SetIsDraftMode(*canvas, is_print_ready_metafile_sent_);
    skia::SetIsPreviewMetafile(*canvas, is_preview);
  }

  float webkit_scale_factor = frame->printPage(page_number, canvas.get());

  if (params.display_header_footer) {
    // |page_number| is 0-based, so 1 is added.
    PrintHeaderAndFooter(canvas.get(), page_number + 1,
                         print_preview_context_.total_page_count(),
                         webkit_scale_factor, page_layout_in_points,
                         *header_footer_info_);
  }

  if (*scale_factor <= 0 || webkit_scale_factor <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page |scale_factor|" calculated in
    // webkit.
    *scale_factor /= webkit_scale_factor;
  }

  bool result = metafile->FinishPage();
  DCHECK(result);

  if (!params.supports_alpha_blend) {
    // PreviewMetafile (PDF) supports alpha blend, so we only hit this case
    // for NativeMetafile.
    DCHECK(!is_preview);
    skia::PlatformDevice* platform_device = skia::GetPlatformDevice(device);
    if (platform_device && platform_device->AlphaBlendUsed()) {
      // Currently, we handle alpha blend transparency for a single page.
      // Therefore, expecting a metafile with page count 1.
      DCHECK_EQ(1U, metafile->GetPageCount());

      // Close the device context to retrieve the compiled metafile.
      if (!metafile->FinishDocument())
        NOTREACHED();

      // Page used alpha blend, but printer doesn't support it.  Rewrite the
      // metafile and flatten out the transparency.
      base::win::ScopedGetDC screen_dc(NULL);
      base::win::ScopedCreateDC bitmap_dc(CreateCompatibleDC(screen_dc));
      if (!bitmap_dc)
        NOTREACHED() << "Bitmap DC creation failed";
      SetGraphicsMode(bitmap_dc, GM_ADVANCED);
      void* bits = NULL;
      BITMAPINFO hdr;
      gfx::CreateBitmapHeader(width, height, &hdr.bmiHeader);
      base::win::ScopedBitmap hbitmap(CreateDIBSection(
          bitmap_dc, &hdr, DIB_RGB_COLORS, &bits, NULL, 0));
      if (!hbitmap)
        NOTREACHED() << "Raster bitmap creation for printing failed";

      base::win::ScopedSelectObject selectBitmap(bitmap_dc, hbitmap);
      RECT rect = {0, 0, width, height };
      HBRUSH whiteBrush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
      FillRect(bitmap_dc, &rect, whiteBrush);

      Metafile* metafile2(new printing::NativeMetafile);
      metafile2->Init();
      HDC hdc = metafile2->context();
      DCHECK(hdc);
      skia::InitializeDC(hdc);

      RECT metafile_bounds = metafile->GetPageBounds(1).ToRECT();
      // Process the old metafile, placing all non-AlphaBlend calls into the
      // new metafile, and copying the results of all the AlphaBlend calls
      // from the bitmap DC.
      EnumEnhMetaFile(hdc,
                      metafile->emf(),
                      EnhMetaFileProc,
                      &bitmap_dc,
                      &metafile_bounds);
      return metafile2;
    }
  }
  return metafile;
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

  Send(new PrintHostMsg_DuplicateSection(routing_id(), *shared_mem_handle,
                                         shared_mem_handle));
  return true;
}
