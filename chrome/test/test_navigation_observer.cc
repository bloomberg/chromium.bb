// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_navigation_observer.h"

#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

// This class observes |rvh| and calls OnJsInjectionReady() of
// |js_injection_ready_observer| when the time is right to inject
// JavaScript into the page.
class TestNavigationObserver::RVHOSendJS
    : public content::RenderViewHostObserver {
 public:
  RVHOSendJS(RenderViewHost* rvh,
             JsInjectionReadyObserver* js_injection_ready_observer)
      : content::RenderViewHostObserver(rvh),
        js_injection_ready_observer_(js_injection_ready_observer) {
  }

 private:
  // content::RenderViewHostObserver implementation.
  virtual void RenderViewHostInitialized() OVERRIDE {
    if (js_injection_ready_observer_)
      js_injection_ready_observer_->OnJsInjectionReady(render_view_host());
  }

  JsInjectionReadyObserver* js_injection_ready_observer_;

  DISALLOW_COPY_AND_ASSIGN(RVHOSendJS);
};

TestNavigationObserver::JsInjectionReadyObserver::JsInjectionReadyObserver() {
}

TestNavigationObserver::JsInjectionReadyObserver::~JsInjectionReadyObserver() {
}

TestNavigationObserver::TestNavigationObserver(
    const content::NotificationSource& source,
    TestNavigationObserver::JsInjectionReadyObserver*
        js_injection_ready_observer,
    int number_of_navigations)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      js_injection_ready_observer_(js_injection_ready_observer),
      done_(false),
      running_(false) {
  // When we need to do javascript injection, register for RVH creation.
  if (js_injection_ready_observer_) {
    registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
                   content::NotificationService::AllSources());
  }
  RegisterAsObserver(source);
}

TestNavigationObserver::~TestNavigationObserver() {
}

void TestNavigationObserver::WaitForObservation() {
  if (!done_) {
    EXPECT_FALSE(running_);
    running_ = true;
    ui_test_utils::RunMessageLoop();
  }
}

TestNavigationObserver::TestNavigationObserver(
    TestNavigationObserver::JsInjectionReadyObserver*
        js_injection_ready_observer,
    int number_of_navigations)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      js_injection_ready_observer_(js_injection_ready_observer),
      done_(false),
      running_(false) {
  // When we need to do javascript injection, register for RVH creation.
  if (js_injection_ready_observer_) {
    registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
                   content::NotificationService::AllSources());
  }
}

void TestNavigationObserver::RegisterAsObserver(
    const content::NotificationSource& source) {
  // Register for events to know when we've finished loading the page and are
  // ready to quit the current message loop to return control back to the
  // waiting test.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);
  registrar_.Add(this, content::NOTIFICATION_LOAD_START, source);
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP, source);
}

void TestNavigationObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
    case content::NOTIFICATION_LOAD_START:
      navigation_started_ = true;
      break;
    case content::NOTIFICATION_LOAD_STOP:
      if (navigation_started_ &&
          ++navigations_completed_ == number_of_navigations_) {
        navigation_started_ = false;
        done_ = true;
        if (running_)
          MessageLoopForUI::current()->Quit();
      }
      break;
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED:
      rvho_send_js_.reset(new RVHOSendJS(
          content::Source<RenderViewHost>(source).ptr(),
          js_injection_ready_observer_));
      break;
    default:
      NOTREACHED();
  }
}
