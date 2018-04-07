// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/layout_and_paint_async_then.h"

#include "base/callback.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/web_layout_and_paint_async_callback.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"

namespace test_runner {

namespace {

class LayoutAndPaintCallback : public blink::WebLayoutAndPaintAsyncCallback {
 public:
  LayoutAndPaintCallback(const base::Closure& callback)
      : callback_(callback), wait_for_popup_(false) {}
  virtual ~LayoutAndPaintCallback() {}

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }

  // WebLayoutAndPaintAsyncCallback implementation.
  void DidLayoutAndPaint() override;

 private:
  base::Closure callback_;
  bool wait_for_popup_;
};

void LayoutAndPaintCallback::DidLayoutAndPaint() {
  TRACE_EVENT0("shell", "LayoutAndPaintCallback::didLayoutAndPaint");
  if (wait_for_popup_) {
    wait_for_popup_ = false;
    return;
  }

  if (!callback_.is_null())
    callback_.Run();
  delete this;
}

}  // namespace

void LayoutAndPaintAsyncThen(blink::WebWidget* web_widget,
                             const base::Closure& callback) {
  TRACE_EVENT0("shell", "LayoutAndPaintAsyncThen");

  LayoutAndPaintCallback* layout_and_paint_callback =
      new LayoutAndPaintCallback(callback);
  web_widget->LayoutAndPaintAsync(layout_and_paint_callback);
  if (blink::WebPagePopup* popup = web_widget->GetPagePopup()) {
    layout_and_paint_callback->set_wait_for_popup(true);
    popup->LayoutAndPaintAsync(layout_and_paint_callback);
  }
}

}  // namespace test_runner
