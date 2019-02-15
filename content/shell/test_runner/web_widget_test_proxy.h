// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_WIDGET_TEST_PROXY_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_WIDGET_TEST_PROXY_H_

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/render_widget.h"
#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebLocalFrame;
class WebString;
}

namespace content {
class RenderViewImpl;
}

namespace test_runner {

class TestRunner;
class TestRunnerForSpecificView;
class EventSender;
class WebViewTestProxy;

// WebWidgetTestProxy is used to run web tests. This class is a partial fake
// implementation of RenderWidget that overrides the minimal necessary portions
// of RenderWidget to allow for use in web tests.
//
// This method of injecting test functionality is an outgrowth of legacy.
// In particular, classic dependency injection does not work easily
// because the RenderWidget class is too large with too much entangled
// stated, and complex creation (subclass using heavy implementation
// inheritance, multiple modes of operation for frames/popups/fullscreen, etc)
// making it hard to factor our creation points for injection.
//
// While implementing a fake via partial overriding of a class leads to
// a fragile base class problem and implicit coupling of the test code
// and production code, it is the most viable mechanism available with a huge
// refactor.
//
// Historically, the overridden functionality has been small enough to not
// cause too much trouble. If that changes, then this entire testing
// architecture should be revisited.
class TEST_RUNNER_EXPORT WebWidgetTestProxy : public content::RenderWidget {
 public:
  template <typename... Args>
  explicit WebWidgetTestProxy(Args&&... args)
      : RenderWidget(std::forward<Args>(args)...) {}

  // WebWidgetClient implementation.
  void ScheduleAnimation() override;
  bool RequestPointerLock() override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override;
  void StartDragging(network::mojom::ReferrerPolicy policy,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const SkBitmap& drag_image,
                     const gfx::Point& image_offset) override;

  // In the test runner code, it can be expected that the RenderViewImpl will
  // actually be a WebViewTestProxy as the creation of RenderView/Frame/Widget
  // are all hooked at the same time to provide a consistent set of fake
  // objects.
  WebViewTestProxy* GetWebViewTestProxy();

  EventSender* event_sender() { return &event_sender_; }
  void Reset();
  void BindTo(blink::WebLocalFrame* frame);

 private:
  // RenderWidget does not have a public destructor.
  ~WebWidgetTestProxy() override;

  TestRunnerForSpecificView* GetViewTestRunner();
  TestRunner* GetTestRunner();

  void AnimateNow();

  EventSender event_sender_{this};

  // For collapsing multiple simulated ScheduleAnimation() calls.
  bool animation_scheduled_ = false;

  base::WeakPtrFactory<WebWidgetTestProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebWidgetTestProxy);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_WIDGET_TEST_PROXY_H_
