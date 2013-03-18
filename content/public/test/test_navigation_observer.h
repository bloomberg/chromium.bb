// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

// For browser_tests, which run on the UI thread, run a second
// MessageLoop and quit when the navigation completes loading.
class TestNavigationObserver : public NotificationObserver {
 public:
  // Create and register a new TestNavigationObserver against the |source|.
  TestNavigationObserver(const NotificationSource& source,
                         int number_of_navigations);
  // Like above but waits for one navigation.
  explicit TestNavigationObserver(const NotificationSource& source);

  virtual ~TestNavigationObserver();

  // Run |wait_loop_callback| until complete, then run |done_callback|.
  void WaitForObservation(const base::Closure& wait_loop_callback,
                          const base::Closure& done_callback);
  // Convenient version of the above that runs a nested message loop and waits.
  void Wait();

 protected:
  explicit TestNavigationObserver(int number_of_navigations);

  // Register this TestNavigationObserver as an observer of the |source|.
  void RegisterAsObserver(const NotificationSource& source);

  // NotificationObserver:
  virtual void Observe(int type, const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  // |done_| will get set when this object observes a TabStripModel event.
  bool done_;

  // |done_callback_| will be set while |running_| is true and will be called
  // when navigation completes.
  base::Closure done_callback_;

  // |running_| will be true during WaitForObservation until |done_| is true.
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
