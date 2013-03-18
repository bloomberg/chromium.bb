// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestNavigationObserver::TestNavigationObserver(
    const NotificationSource& source,
    int number_of_navigations)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      done_(false),
      running_(false) {
  RegisterAsObserver(source);
}

TestNavigationObserver::TestNavigationObserver(
    const NotificationSource& source)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(1),
      done_(false),
      running_(false) {
  RegisterAsObserver(source);
}

TestNavigationObserver::~TestNavigationObserver() {
}

void TestNavigationObserver::WaitForObservation(
    const base::Closure& wait_loop_callback,
    const base::Closure& done_callback) {
  if (done_)
    return;

  EXPECT_FALSE(running_);
  running_ = true;
  done_callback_ = done_callback;
  wait_loop_callback.Run();
  EXPECT_TRUE(done_);
}

void TestNavigationObserver::Wait() {
  base::RunLoop run_loop;
  WaitForObservation(
      base::Bind(&base::RunLoop::Run, base::Unretained(&run_loop)),
      GetQuitTaskForRunLoop(&run_loop));
}

TestNavigationObserver::TestNavigationObserver(
    int number_of_navigations)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      done_(false),
      running_(false) {
}

void TestNavigationObserver::RegisterAsObserver(
    const NotificationSource& source) {
  // Register for events to know when we've finished loading the page and are
  // ready to quit the current message loop to return control back to the
  // waiting test.
  registrar_.Add(this, NOTIFICATION_NAV_ENTRY_COMMITTED, source);
  registrar_.Add(this, NOTIFICATION_LOAD_START, source);
  registrar_.Add(this, NOTIFICATION_LOAD_STOP, source);
}

void TestNavigationObserver::Observe(
    int type, const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_NAV_ENTRY_COMMITTED:
    case NOTIFICATION_LOAD_START:
      navigation_started_ = true;
      break;
    case NOTIFICATION_LOAD_STOP:
      if (navigation_started_ &&
          ++navigations_completed_ == number_of_navigations_) {
        navigation_started_ = false;
        done_ = true;
        if (running_) {
          running_ = false;
          done_callback_.Run();
        }
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace content
