// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/pixel_dump.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "content/shell/test_runner/layout_test_runtime_flags.h"
// FIXME: Including platform_canvas.h here is a layering violation.
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebMockClipboard.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/point.h"

namespace test_runner {

namespace {

struct PixelsDumpRequest {
  PixelsDumpRequest(blink::WebView* web_view,
                    const LayoutTestRuntimeFlags& layout_test_runtime_flags,
                    const base::Callback<void(const SkBitmap&)>& callback)
      : web_view(web_view),
        layout_test_runtime_flags(layout_test_runtime_flags),
        callback(callback) {}

  blink::WebView* web_view;
  const LayoutTestRuntimeFlags& layout_test_runtime_flags;
  base::Callback<void(const SkBitmap&)> callback;
};

class CaptureCallback : public blink::WebCompositeAndReadbackAsyncCallback {
 public:
  CaptureCallback(const base::Callback<void(const SkBitmap&)>& callback);
  virtual ~CaptureCallback();

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }
  void set_popup_position(const gfx::Point& position) {
    popup_position_ = position;
  }

  // WebCompositeAndReadbackAsyncCallback implementation.
  void didCompositeAndReadback(const SkBitmap& bitmap) override;

 private:
  base::Callback<void(const SkBitmap&)> callback_;
  SkBitmap main_bitmap_;
  bool wait_for_popup_;
  gfx::Point popup_position_;
};

void DrawSelectionRect(const PixelsDumpRequest& dump_request,
                       cc::PaintCanvas* canvas) {
  // See if we need to draw the selection bounds rect. Selection bounds
  // rect is the rect enclosing the (possibly transformed) selection.
  // The rect should be drawn after everything is laid out and painted.
  if (!dump_request.layout_test_runtime_flags.dump_selection_rect())
    return;
  // If there is a selection rect - draw a red 1px border enclosing rect
  blink::WebRect wr = dump_request.web_view->mainFrame()->selectionBoundsRect();
  if (wr.isEmpty())
    return;
  // Render a red rectangle bounding selection rect
  cc::PaintFlags flags;
  flags.setColor(0xFFFF0000);  // Fully opaque red
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setStrokeWidth(1.0f);
  SkIRect rect;  // Bounding rect
  rect.set(wr.x, wr.y, wr.x + wr.width, wr.y + wr.height);
  canvas->drawIRect(rect, flags);
}

void CapturePixelsForPrinting(std::unique_ptr<PixelsDumpRequest> dump_request) {
  dump_request->web_view->updateAllLifecyclePhases();

  blink::WebSize page_size_in_pixels = dump_request->web_view->size();
  blink::WebFrame* web_frame = dump_request->web_view->mainFrame();

  int page_count = web_frame->printBegin(page_size_in_pixels);
  int totalHeight = page_count * (page_size_in_pixels.height + 1) - 1;

  bool is_opaque = false;

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(page_size_in_pixels.width, totalHeight,
                                is_opaque)) {
    LOG(ERROR) << "Failed to create bitmap width=" << page_size_in_pixels.width
               << " height=" << totalHeight;
    dump_request->callback.Run(SkBitmap());
    return;
  }

  cc::PaintCanvas canvas(bitmap);
  web_frame->printPagesWithBoundaries(&canvas, page_size_in_pixels);
  web_frame->printEnd();

  DrawSelectionRect(*dump_request, &canvas);
  dump_request->callback.Run(bitmap);
}

CaptureCallback::CaptureCallback(
    const base::Callback<void(const SkBitmap&)>& callback)
    : callback_(callback), wait_for_popup_(false) {}

CaptureCallback::~CaptureCallback() {}

void CaptureCallback::didCompositeAndReadback(const SkBitmap& bitmap) {
  TRACE_EVENT2("shell", "CaptureCallback::didCompositeAndReadback", "x",
               bitmap.info().width(), "y", bitmap.info().height());
  if (!wait_for_popup_) {
    callback_.Run(bitmap);
    delete this;
    return;
  }
  if (main_bitmap_.isNull()) {
    bitmap.deepCopyTo(&main_bitmap_);
    return;
  }
  SkCanvas canvas(main_bitmap_);
  canvas.drawBitmap(bitmap, popup_position_.x(), popup_position_.y());
  callback_.Run(main_bitmap_);
  delete this;
}

void DidCapturePixelsAsync(std::unique_ptr<PixelsDumpRequest> dump_request,
                           const SkBitmap& bitmap) {
  cc::PaintCanvas canvas(bitmap);
  DrawSelectionRect(*dump_request, &canvas);
  if (!dump_request->callback.is_null())
    dump_request->callback.Run(bitmap);
}

}  // namespace

void DumpPixelsAsync(blink::WebView* web_view,
                     const LayoutTestRuntimeFlags& layout_test_runtime_flags,
                     float device_scale_factor_for_test,
                     const base::Callback<void(const SkBitmap&)>& callback) {
  TRACE_EVENT0("shell", "WebViewTestProxyBase::CapturePixelsAsync");
  DCHECK(!callback.is_null());
  DCHECK(!layout_test_runtime_flags.dump_drag_image());

  std::unique_ptr<PixelsDumpRequest> pixels_request(
      new PixelsDumpRequest(web_view, layout_test_runtime_flags, callback));

  if (layout_test_runtime_flags.is_printing()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&CapturePixelsForPrinting,
                              base::Passed(std::move(pixels_request))));
    return;
  }

  CaptureCallback* capture_callback = new CaptureCallback(base::Bind(
      &DidCapturePixelsAsync, base::Passed(std::move(pixels_request))));
  web_view->compositeAndReadbackAsync(capture_callback);
  if (blink::WebPagePopup* popup = web_view->pagePopup()) {
    capture_callback->set_wait_for_popup(true);
    blink::WebPoint position = popup->positionRelativeToOwner();
    position.x *= device_scale_factor_for_test;
    position.y *= device_scale_factor_for_test;
    capture_callback->set_popup_position(position);
    popup->compositeAndReadbackAsync(capture_callback);
  }
}

void CopyImageAtAndCapturePixels(
    blink::WebView* web_view,
    int x,
    int y,
    const base::Callback<void(const SkBitmap&)>& callback) {
  DCHECK(!callback.is_null());
  uint64_t sequence_number =
      blink::Platform::current()->clipboard()->sequenceNumber(
          blink::WebClipboard::Buffer());
  // TODO(lukasza): Support image capture in OOPIFs for
  // https://crbug.com/477150.
  web_view->mainFrame()->toWebLocalFrame()->copyImageAt(blink::WebPoint(x, y));
  if (sequence_number ==
      blink::Platform::current()->clipboard()->sequenceNumber(
          blink::WebClipboard::Buffer())) {
    SkBitmap emptyBitmap;
    callback.Run(emptyBitmap);
    return;
  }

  blink::WebImage image = static_cast<blink::WebMockClipboard*>(
                              blink::Platform::current()->clipboard())
                              ->readRawImage(blink::WebClipboard::Buffer());
  const SkBitmap& bitmap = image.getSkBitmap();
  SkAutoLockPixels autoLock(bitmap);
  callback.Run(bitmap);
}

}  // namespace test_runner
