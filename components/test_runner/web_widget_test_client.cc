// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_widget_test_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/test_runner_for_specific_view.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebWidget.h"

namespace test_runner {

WebWidgetTestClient::WebWidgetTestClient(TestRunner* test_runner,
                                         WebTestProxyBase* web_test_proxy_base)
    : test_runner_(test_runner),
      web_test_proxy_base_(web_test_proxy_base),
      animation_scheduled_(false),
      weak_factory_(this) {
  DCHECK(test_runner);
  DCHECK(web_test_proxy_base);
}

WebWidgetTestClient::~WebWidgetTestClient() {}

void WebWidgetTestClient::scheduleAnimation() {
  if (!test_runner_->TestIsRunning())
    return;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;
    test_runner_->OnAnimationScheduled(web_test_proxy_base_->web_widget());

    web_test_proxy_base_->delegate()->PostDelayedTask(
        new WebCallbackTask(base::Bind(&WebWidgetTestClient::AnimateNow,
                                       weak_factory_.GetWeakPtr())),
        1);
  }
}

void WebWidgetTestClient::AnimateNow() {
  if (animation_scheduled_) {
    blink::WebWidget* web_widget = web_test_proxy_base_->web_widget();
    animation_scheduled_ = false;
    test_runner_->OnAnimationBegun(web_widget);

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
      web_test_proxy_base_->test_interfaces()
          ->GetTestRunner()
          ->getMockScreenOrientationClient();
  if (mock_client->IsDisabled()) {
    // Indicate to WebTestProxy that there is no test/mock info.
    screen_info.orientationType = blink::WebScreenOrientationUndefined;
  } else {
    // Override screen orientation information with mock data.
    screen_info.orientationType = mock_client->CurrentOrientationType();
    screen_info.orientationAngle = mock_client->CurrentOrientationAngle();
  }
  return screen_info;
}

bool WebWidgetTestClient::requestPointerLock() {
  return web_test_proxy_base_->view_test_runner()->RequestPointerLock();
}

void WebWidgetTestClient::requestPointerUnlock() {
  web_test_proxy_base_->view_test_runner()->RequestPointerUnlock();
}

bool WebWidgetTestClient::isPointerLocked() {
  return web_test_proxy_base_->view_test_runner()->isPointerLocked();
}

void WebWidgetTestClient::didFocus() {
  test_runner_->SetFocus(web_test_proxy_base_->web_view(), true);
}

void WebWidgetTestClient::setToolTipText(const blink::WebString& text,
                                         blink::WebTextDirection direction) {
  test_runner_->setToolTipText(text);
}

void WebWidgetTestClient::resetInputMethod() {
  // If a composition text exists, then we need to let the browser process
  // to cancel the input method's ongoing composition session.
  if (web_test_proxy_base_)
    web_test_proxy_base_->web_widget()->confirmComposition();
}

}  // namespace test_runner
