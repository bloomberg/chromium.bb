// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/automation/automation_renderer_helper.h"

#include <algorithm>

#include "base/basictypes.h"
#include "chrome/common/automation_messages.h"
#include "content/public/renderer/render_view.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebSize;
using WebKit::WebURL;
using WebKit::WebView;

AutomationRendererHelper::AutomationRendererHelper(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
}

AutomationRendererHelper::~AutomationRendererHelper() { }

bool AutomationRendererHelper::SnapshotEntirePage(
    WebView* view,
    std::vector<unsigned char>* png_data,
    std::string* error_msg) {
  WebFrame* frame = view->mainFrame();
  WebSize old_size = view->size();
  WebSize new_size = frame->contentsSize();
  // For RTL, the minimum scroll offset may be negative.
  WebSize min_scroll = frame->minimumScrollOffset();
  WebSize old_scroll = frame->scrollOffset();
  bool fixed_layout_enabled = view->isFixedLayoutModeEnabled();
  WebSize fixed_size = view->fixedLayoutSize();

  frame->setCanHaveScrollbars(false);
  view->setFixedLayoutSize(old_size);
  view->enableFixedLayoutMode(true);
  view->resize(new_size);
  view->layout();
  frame->setScrollOffset(WebSize(0, 0));

  skia::PlatformCanvas canvas(
      new_size.width, new_size.height, true /* is_opaque */);
  view->paint(webkit_glue::ToWebCanvas(&canvas),
              gfx::Rect(0, 0, new_size.width, new_size.height));

  frame->setCanHaveScrollbars(true);
  view->setFixedLayoutSize(fixed_size);
  view->enableFixedLayoutMode(fixed_layout_enabled);
  view->resize(old_size);
  view->layout();
  frame->setScrollOffset(WebSize(old_scroll.width - min_scroll.width,
                                 old_scroll.height - min_scroll.height));

  const SkBitmap& bmp = skia::GetTopDevice(canvas)->accessBitmap(false);
  SkAutoLockPixels lock_pixels(bmp);
  // EncodeBGRA uses FORMAT_SkBitmap, which doesn't work on windows for some
  // cases dealing with transparency. See crbug.com/96317. Use FORMAT_BGRA.
  bool encode_success = gfx::PNGCodec::Encode(
      reinterpret_cast<unsigned char*>(bmp.getPixels()),
      gfx::PNGCodec::FORMAT_BGRA,
      gfx::Size(bmp.width(), bmp.height()),
      bmp.rowBytes(),
      true,  // discard_transparency
      std::vector<gfx::PNGCodec::Comment>(),
      png_data);
  if (!encode_success)
    *error_msg = "failed to encode image as png";
  return encode_success;
}

void AutomationRendererHelper::OnSnapshotEntirePage() {
  std::vector<unsigned char> png_data;
  std::string error_msg;
  bool success = false;
  if (render_view()->GetWebView()) {
    success = SnapshotEntirePage(
        render_view()->GetWebView(), &png_data, &error_msg);
  } else {
    error_msg = "cannot snapshot page because webview is null";
  }

  // Check that the image is not too large, allowing a 1kb buffer for other
  // message data.
  if (success && png_data.size() > IPC::Channel::kMaximumMessageSize - 1024) {
    png_data.clear();
    success = false;
    error_msg = "image is too large to be transferred over ipc";
  }
  Send(new AutomationMsg_SnapshotEntirePageACK(
      routing_id(), success, png_data, error_msg));
}

bool AutomationRendererHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool deserialize_success = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AutomationRendererHelper, message,
                           deserialize_success)
    IPC_MESSAGE_HANDLER(AutomationMsg_SnapshotEntirePage, OnSnapshotEntirePage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  if (!deserialize_success) {
    LOG(ERROR) << "Failed to deserialize an IPC message";
  }
  return handled;
}

void AutomationRendererHelper::WillPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to, double interval,
    double fire_time) {
  Send(new AutomationMsg_WillPerformClientRedirect(
      routing_id(), frame->identifier(), interval));
}

void AutomationRendererHelper::DidCancelClientRedirect(WebFrame* frame) {
  Send(new AutomationMsg_DidCompleteOrCancelClientRedirect(
      routing_id(), frame->identifier()));
}

void AutomationRendererHelper::DidCompleteClientRedirect(
    WebFrame* frame, const WebURL& from) {
  Send(new AutomationMsg_DidCompleteOrCancelClientRedirect(
      routing_id(), frame->identifier()));
}
