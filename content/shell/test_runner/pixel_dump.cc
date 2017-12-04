// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/pixel_dump.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/shell/test_runner/layout_test_runtime_flags.h"
// FIXME: Including platform_canvas.h here is a layering violation.
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebMockClipboard.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "ui/gfx/geometry/point.h"

namespace test_runner {

namespace {

class CaptureCallback : public blink::WebCompositeAndReadbackAsyncCallback {
 public:
  explicit CaptureCallback(base::OnceCallback<void(const SkBitmap&)> callback);
  virtual ~CaptureCallback();

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }
  void set_popup_position(const gfx::Point& position) {
    popup_position_ = position;
  }

  // WebCompositeAndReadbackAsyncCallback implementation.
  void DidCompositeAndReadback(const SkBitmap& bitmap) override;

 private:
  base::OnceCallback<void(const SkBitmap&)> callback_;
  SkBitmap main_bitmap_;
  bool wait_for_popup_;
  gfx::Point popup_position_;
};

void DrawSelectionRect(
    const blink::WebRect& wr,
    base::OnceCallback<void(const SkBitmap&)> original_callback,
    const SkBitmap& bitmap) {
  // Render a red rectangle bounding selection rect
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setColor(0xFFFF0000);  // Fully opaque red
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setStrokeWidth(1.0f);
  SkIRect rect;  // Bounding rect
  rect.set(wr.x, wr.y, wr.x + wr.width, wr.y + wr.height);
  canvas.drawIRect(rect, flags);

  std::move(original_callback).Run(bitmap);
}

void CapturePixelsForPrinting(
    blink::WebLocalFrame* web_frame,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  web_frame->FrameWidget()->UpdateAllLifecyclePhases();

  blink::WebSize page_size_in_pixels = web_frame->FrameWidget()->Size();

  int page_count = web_frame->PrintBegin(page_size_in_pixels);
  int totalHeight = page_count * (page_size_in_pixels.height + 1) - 1;

  bool is_opaque = false;

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(page_size_in_pixels.width, totalHeight,
                                is_opaque)) {
    LOG(ERROR) << "Failed to create bitmap width=" << page_size_in_pixels.width
               << " height=" << totalHeight;
    std::move(callback).Run(SkBitmap());
    return;
  }

  cc::SkiaPaintCanvas canvas(bitmap);
  web_frame->PrintPagesForTesting(&canvas, page_size_in_pixels);
  web_frame->PrintEnd();

  std::move(callback).Run(bitmap);
}

CaptureCallback::CaptureCallback(
    base::OnceCallback<void(const SkBitmap&)> callback)
    : callback_(std::move(callback)), wait_for_popup_(false) {}

CaptureCallback::~CaptureCallback() {}

void CaptureCallback::DidCompositeAndReadback(const SkBitmap& bitmap) {
  TRACE_EVENT2("shell", "CaptureCallback::didCompositeAndReadback", "x",
               bitmap.info().width(), "y", bitmap.info().height());
  if (!wait_for_popup_) {
    std::move(callback_).Run(bitmap);
    delete this;
    return;
  }
  if (main_bitmap_.isNull()) {
    if (main_bitmap_.tryAllocPixels(bitmap.info())) {
      bitmap.readPixels(main_bitmap_.info(), main_bitmap_.getPixels(),
                        main_bitmap_.rowBytes(), 0, 0);
    }
    return;
  }
  SkCanvas canvas(main_bitmap_);
  canvas.drawBitmap(bitmap, popup_position_.x(), popup_position_.y());
  std::move(callback_).Run(main_bitmap_);
  delete this;
}

}  // namespace

void DumpPixelsAsync(blink::WebLocalFrame* web_frame,
                     float device_scale_factor_for_test,
                     base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(web_frame);
  DCHECK_LT(0.0, device_scale_factor_for_test);
  DCHECK(!callback.is_null());

  blink::WebWidget* web_widget = web_frame->FrameWidget();
  CaptureCallback* capture_callback = new CaptureCallback(std::move(callback));
  web_widget->CompositeAndReadbackAsync(capture_callback);
  if (blink::WebPagePopup* popup = web_widget->GetPagePopup()) {
    capture_callback->set_wait_for_popup(true);
    blink::WebPoint position = popup->PositionRelativeToOwner();
    position.x *= device_scale_factor_for_test;
    position.y *= device_scale_factor_for_test;
    capture_callback->set_popup_position(position);
    popup->CompositeAndReadbackAsync(capture_callback);
  }
}

void PrintFrameAsync(blink::WebLocalFrame* web_frame,
                     base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(web_frame);
  DCHECK(!callback.is_null());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CapturePixelsForPrinting, base::Unretained(web_frame),
                     base::Passed(std::move(callback))));
}

base::OnceCallback<void(const SkBitmap&)>
CreateSelectionBoundsRectDrawingCallback(
    blink::WebLocalFrame* web_frame,
    base::OnceCallback<void(const SkBitmap&)> original_callback) {
  DCHECK(web_frame);
  DCHECK(!original_callback.is_null());

  // If there is no selection rect, just return the original callback.
  blink::WebRect wr = web_frame->GetSelectionBoundsRectForTesting();
  if (wr.IsEmpty())
    return original_callback;

  return base::BindOnce(&DrawSelectionRect, wr,
                        base::Passed(std::move(original_callback)));
}

void CopyImageAtAndCapturePixels(
    blink::WebLocalFrame* web_frame,
    int x,
    int y,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(!callback.is_null());
  uint64_t sequence_number =
      blink::Platform::Current()->Clipboard()->SequenceNumber(
          blink::mojom::ClipboardBuffer::kStandard);
  web_frame->CopyImageAt(blink::WebPoint(x, y));
  if (sequence_number ==
      blink::Platform::Current()->Clipboard()->SequenceNumber(
          blink::mojom::ClipboardBuffer::kStandard)) {
    SkBitmap emptyBitmap;
    std::move(callback).Run(emptyBitmap);
    return;
  }

  blink::WebImage image =
      static_cast<blink::WebMockClipboard*>(
          blink::Platform::Current()->Clipboard())
          ->ReadRawImage(blink::mojom::ClipboardBuffer::kStandard);
  std::move(callback).Run(image.GetSkBitmap());
}

}  // namespace test_runner
