// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_navigation_observer.h"

#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TestNavigationObserver::JsInjectionReadyObserver::JsInjectionReadyObserver() {
}

TestNavigationObserver::JsInjectionReadyObserver::~JsInjectionReadyObserver() {
}

TestNavigationObserver::TestNavigationObserver(
    NavigationController* controller,
    TestNavigationObserver::JsInjectionReadyObserver*
        js_injection_ready_observer,
    int number_of_navigations)
    : navigation_started_(false),
      navigation_entry_committed_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      js_injection_ready_observer_(js_injection_ready_observer),
      done_(false),
      running_(false) {
  RegisterAsObserver(controller);
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
      navigation_entry_committed_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      js_injection_ready_observer_(js_injection_ready_observer),
      done_(false),
      running_(false) {
}

void TestNavigationObserver::RegisterAsObserver(
    NavigationController* controller) {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 Source<NavigationController>(controller));
}

void TestNavigationObserver::Observe(
    int type, const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
      if (!navigation_entry_committed_ && js_injection_ready_observer_)
        js_injection_ready_observer_->OnJsInjectionReady();
      navigation_started_ = true;
      navigation_entry_committed_ = true;
      break;
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
    default:
      NOTREACHED();
  }
}
