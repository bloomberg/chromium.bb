// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_runner.h"

#include <cmath>

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/shell_messages.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#ifdef WEBKIT_TESTING_SUPPORT_AVAILABLE
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTestingSupport.h"
#endif
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebElement;
using WebKit::WebRect;
using WebKit::WebSize;
#ifdef WEBKIT_TESTING_SUPPORT_AVAILABLE
using WebKit::WebTestingSupport;
#endif
using WebKit::WebView;

namespace content {

namespace {

std::string DumpDocumentText(WebFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebElement documentElement = frame->document().documentElement();
  if (documentElement.isNull())
    return std::string();
  return documentElement.innerText().utf8();
}

std::string DumpDocumentPrintedText(WebFrame* frame) {
  return frame->renderTreeAsText(WebFrame::RenderAsTextPrinting).utf8();
}

std::string DumpFramesAsText(WebFrame* frame, bool printing, bool recursive) {
  std::string result;

  // Cannot do printed format for anything other than HTML.
  if (printing && !frame->document().isHTMLDocument())
    return std::string();

  // Add header for all but the main frame. Skip emtpy frames.
  if (frame->parent() && !frame->document().documentElement().isNull()) {
    result.append("\n--------\nFrame: '");
    result.append(frame->uniqueName().utf8().data());
    result.append("'\n--------\n");
  }

  result.append(
      printing ? DumpDocumentPrintedText(frame) : DumpDocumentText(frame));
  result.append("\n");

  if (recursive) {
    for (WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling()) {
      result.append(DumpFramesAsText(child, printing, recursive));
    }
  }
  return result;
}

std::string DumpFrameScrollPosition(WebFrame* frame, bool recursive) {
  std::string result;

  WebSize offset = frame->scrollOffset();
  if (offset.width > 0 || offset.height > 0) {
    if (frame->parent()) {
      result.append(
          base::StringPrintf("frame '%s' ", frame->uniqueName().utf8().data()));
    }
    result.append(
        base::StringPrintf("scrolled to %d,%d\n", offset.width, offset.height));
  }

  if (recursive) {
    for (WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling()) {
      result.append(DumpFrameScrollPosition(child, recursive));
    }
  }
  return result;
}

#if !defined(OS_MACOSX)
void MakeBitmapOpaque(SkBitmap* bitmap) {
  SkAutoLockPixels lock(*bitmap);
  DCHECK(bitmap->config() == SkBitmap::kARGB_8888_Config);
  for (int y = 0; y < bitmap->height(); ++y) {
    uint32_t* row = bitmap->getAddr32(0, y);
    for (int x = 0; x < bitmap->width(); ++x)
      row[x] |= 0xFF000000;  // Set alpha bits to 1.
  }
}
#endif

void CopyCanvasToBitmap(skia::PlatformCanvas* canvas, SkBitmap* snapshot) {
  SkDevice* device = skia::GetTopDevice(*canvas);

  const SkBitmap& bitmap = device->accessBitmap(false);
  bitmap.copyTo(snapshot, SkBitmap::kARGB_8888_Config);

#if !defined(OS_MACOSX)
  // Only the expected PNGs for Mac have a valid alpha channel.
  MakeBitmapOpaque(snapshot);
#endif

}

}  // namespace

WebKitTestRunner::WebKitTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<WebKitTestRunner>(render_view) {
}

WebKitTestRunner::~WebKitTestRunner() {
}

void WebKitTestRunner::DidClearWindowObject(WebFrame* frame) {
#ifdef WEBKIT_TESTING_SUPPORT_AVAILABLE
  WebTestingSupport::injectInternalsObject(frame);
#endif
}

void WebKitTestRunner::DidFinishLoad(WebFrame* frame) {
  if (!frame->parent())
    Send(new ShellViewHostMsg_DidFinishLoad(routing_id()));
}

bool WebKitTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestRunner, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_CaptureTextDump, OnCaptureTextDump)
    IPC_MESSAGE_HANDLER(ShellViewMsg_CaptureImageDump, OnCaptureImageDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestRunner::DidInvalidateRect(const WebRect& rect) {
  UpdatePaintRect(rect);
}

void WebKitTestRunner::DidScrollRect(int dx, int dy, const WebRect& rect) {
  DidInvalidateRect(rect);
}

void WebKitTestRunner::DidRequestScheduleComposite() {
  const WebSize& size = render_view()->GetWebView()->size();
  WebRect rect(0, 0, size.width, size.height);
  DidInvalidateRect(rect);
}

void WebKitTestRunner::DidRequestScheduleAnimation() {
  DidRequestScheduleComposite();
}

void WebKitTestRunner::Display() {
  const WebSize& size = render_view()->GetWebView()->size();
  WebRect rect(0, 0, size.width, size.height);
  UpdatePaintRect(rect);
  PaintInvalidatedRegion();
  DisplayRepaintMask();
}

void WebKitTestRunner::OnCaptureTextDump(bool as_text,
                                         bool printing,
                                         bool recursive) {
  WebFrame* frame = render_view()->GetWebView()->mainFrame();
  std::string dump;
  if (as_text) {
    dump = DumpFramesAsText(frame, printing, recursive);
  } else {
    WebFrame::RenderAsTextControls render_text_behavior =
        WebFrame::RenderAsTextNormal;
    if (printing)
      render_text_behavior |= WebFrame::RenderAsTextPrinting;
    dump = frame->renderTreeAsText(render_text_behavior).utf8();
    dump.append(DumpFrameScrollPosition(frame, recursive));
  }
  Send(new ShellViewHostMsg_TextDump(routing_id(), dump));
}

void WebKitTestRunner::OnCaptureImageDump(
    const std::string& expected_pixel_hash) {
  SkBitmap snapshot;
  PaintInvalidatedRegion();
  CopyCanvasToBitmap(GetCanvas(), &snapshot);

  SkAutoLockPixels snapshot_lock(snapshot);
  base::MD5Digest digest;
#if defined(OS_ANDROID)
  // On Android, pixel layout is RGBA, however, other Chrome platforms use BGRA.
  const uint8_t* raw_pixels =
      reinterpret_cast<const uint8_t*>(snapshot.getPixels());
  size_t snapshot_size = snapshot.getSize();
  scoped_array<uint8_t> reordered_pixels(new uint8_t[snapshot_size]);
  for (size_t i = 0; i < snapshot_size; i += 4) {
    reordered_pixels[i] = raw_pixels[i + 2];
    reordered_pixels[i + 1] = raw_pixels[i + 1];
    reordered_pixels[i + 2] = raw_pixels[i];
    reordered_pixels[i + 3] = raw_pixels[i + 3];
  }
  base::MD5Sum(reordered_pixels.get(), snapshot_size, &digest);
#else
  base::MD5Sum(snapshot.getPixels(), snapshot.getSize(), &digest);
#endif
  std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

  if (actual_pixel_hash == expected_pixel_hash) {
    SkBitmap empty_image;
    Send(new ShellViewHostMsg_ImageDump(
        routing_id(), actual_pixel_hash, empty_image));
    return;
  }
  Send(new ShellViewHostMsg_ImageDump(
      routing_id(), actual_pixel_hash, snapshot));
}

skia::PlatformCanvas* WebKitTestRunner::GetCanvas() {
  if (canvas_)
    return canvas_.get();

  WebView* view = render_view()->GetWebView();
  const WebSize& size = view->size();
  float device_scale_factor = view->deviceScaleFactor();
  canvas_.reset(new skia::PlatformCanvas);
  canvas_->initialize(std::ceil(device_scale_factor * size.width),
                      std::ceil(device_scale_factor * size.height),
                      true);
  return canvas_.get();
}

void WebKitTestRunner::UpdatePaintRect(const WebRect& rect) {
  // Update paint_rect_ to the smallest rectangle that covers both rect and
  // paint_rect_.
  if (rect.isEmpty())
    return;
  if (paint_rect_.isEmpty()) {
    paint_rect_ = rect;
    return;
  }
  int left = std::min(paint_rect_.x, rect.x);
  int top = std::min(paint_rect_.y, rect.y);
  int right = std::max(paint_rect_.x + paint_rect_.width, rect.x + rect.width);
  int bottom = std::max(paint_rect_.y + paint_rect_.height,
                        rect.y + rect.height);
  paint_rect_ = WebRect(left, top, right - left, bottom - top);
}

void WebKitTestRunner::PaintRect(const WebRect& rect) {
  WebView* view = render_view()->GetWebView();
  float device_scale_factor = view->deviceScaleFactor();
  int scaled_x = device_scale_factor * rect.x;
  int scaled_y = device_scale_factor * rect.y;
  int scaled_width = std::ceil(device_scale_factor * rect.width);
  int scaled_height = std::ceil(device_scale_factor * rect.height);
  // TODO(jochen): Verify that the scaling is correct once the HiDPI tests
  // actually work.
  WebRect device_rect(scaled_x, scaled_y, scaled_width, scaled_height);
  view->paint(webkit_glue::ToWebCanvas(GetCanvas()), device_rect);
}

void WebKitTestRunner::PaintInvalidatedRegion() {
  WebView* view = render_view()->GetWebView();
  view->animate(0.0);
  view->layout();
  const WebSize& widget_size = view->size();
  WebRect client_rect(0, 0, widget_size.width, widget_size.height);

  // Paint the canvas if necessary. Allow painting to generate extra rects
  // for the first two calls. This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 3; ++i) {
    // Make sure that paint_rect_ is always inside the RenderView's visible
    // area.
    int left = std::max(paint_rect_.x, client_rect.x);
    int top = std::max(paint_rect_.y, client_rect.y);
    int right = std::min(paint_rect_.x + paint_rect_.width,
                         client_rect.x + client_rect.width);
    int bottom = std::min(paint_rect_.y + paint_rect_.height,
                          client_rect.y + client_rect.height);
    if (left >= right || top >= bottom)
      paint_rect_ = WebRect();
    else
      paint_rect_ = WebRect(left, top, right - left, bottom - top);
    if (paint_rect_.isEmpty())
      continue;
    WebRect rect(paint_rect_);
    paint_rect_ = WebRect();
    PaintRect(rect);
  }
  CHECK(paint_rect_.isEmpty());
}

void WebKitTestRunner::DisplayRepaintMask() {
  GetCanvas()->drawARGB(167, 0, 0, 0);
}

}  // namespace content
