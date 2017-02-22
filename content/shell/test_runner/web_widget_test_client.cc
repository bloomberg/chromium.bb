// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_widget_test_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebWidget.h"

namespace test_runner {

WebWidgetTestClient::WebWidgetTestClient(
    WebWidgetTestProxyBase* web_widget_test_proxy_base)
    : web_widget_test_proxy_base_(web_widget_test_proxy_base),
      animation_scheduled_(false),
      weak_factory_(this) {
  DCHECK(web_widget_test_proxy_base_);
}

WebWidgetTestClient::~WebWidgetTestClient() {}

void WebWidgetTestClient::scheduleAnimation() {
  if (!test_runner()->TestIsRunning())
    return;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;
    delegate()->PostDelayedTask(base::Bind(&WebWidgetTestClient::AnimateNow,
                                           weak_factory_.GetWeakPtr()),
                                1);
  }
}

void WebWidgetTestClient::AnimateNow() {
  if (animation_scheduled_) {
    blink::WebWidget* web_widget = web_widget_test_proxy_base_->web_widget();
    animation_scheduled_ = false;
    base::TimeDelta animate_time = base::TimeTicks::Now() - base::TimeTicks();
    web_widget->beginFrame(animate_time.InSecondsF());
    web_widget->updateAllLifecyclePhases();
    if (blink::WebPagePopup* popup = web_widget->pagePopup()) {
      popup->beginFrame(animate_time.InSecondsF());
      popup->updateAllLifecyclePhases();
    }
  }
}

blink::WebScreenInfo WebWidgetTestClient::screenInfo() {
  blink::WebScreenInfo screen_info;
  MockScreenOrientationClient* mock_client =
      test_runner()->getMockScreenOrientationClient();
  if (mock_client->IsDisabled()) {
    // Indicate to WebViewTestProxy that there is no test/mock info.
    screen_info.orientationType = blink::WebScreenOrientationUndefined;
  } else {
    // Override screen orientation information with mock data.
    screen_info.orientationType = mock_client->CurrentOrientationType();
    screen_info.orientationAngle = mock_client->CurrentOrientationAngle();
  }
  return screen_info;
}

bool WebWidgetTestClient::requestPointerLock() {
  return view_test_runner()->RequestPointerLock();
}

void WebWidgetTestClient::requestPointerUnlock() {
  view_test_runner()->RequestPointerUnlock();
}

bool WebWidgetTestClient::isPointerLocked() {
  return view_test_runner()->isPointerLocked();
}

void WebWidgetTestClient::setToolTipText(const blink::WebString& text,
                                         blink::WebTextDirection direction) {
  test_runner()->setToolTipText(text);
}

void WebWidgetTestClient::startDragging(blink::WebReferrerPolicy policy,
                                        const blink::WebDragData& data,
                                        blink::WebDragOperationsMask mask,
                                        const blink::WebImage& image,
                                        const blink::WebPoint& point) {
  test_runner()->setDragImage(image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  web_widget_test_proxy_base_->event_sender()->DoDragDrop(data, mask);
}

TestRunnerForSpecificView* WebWidgetTestClient::view_test_runner() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()
      ->view_test_runner();
}

WebTestDelegate* WebWidgetTestClient::delegate() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()->delegate();
}

TestRunner* WebWidgetTestClient::test_runner() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()
      ->test_interfaces()
      ->GetTestRunner();
}

}  // namespace test_runner
