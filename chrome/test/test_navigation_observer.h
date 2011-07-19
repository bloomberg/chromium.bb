// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_NAVIGATION_OBSERVER_H_
#define CHROME_TEST_TEST_NAVIGATION_OBSERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class NavigationController;

// In order to support testing of print preview, we need to wait for the tab to
// be inserted, and then observe notifications on the newly added tab's
// controller to wait for it to be loaded. To support tests registering
// javascript WebUI handlers, we need to inject the framework & registration
// javascript before the webui page loads by calling back through the
// TestTabStripModelObserver::LoadStartObserver when the new page starts
// loading.
class TestNavigationObserver : public NotificationObserver {
 public:
  class JsInjectionReadyObserver {
   public:
    JsInjectionReadyObserver();
    virtual ~JsInjectionReadyObserver();

    // Called to indicate page entry committed and ready for javascript
    // injection.
    virtual void OnJsInjectionReady() = 0;
  };

  // Create and register a new TestNavigationObserver against the
  // |controller|. When |js_injection_ready_observer| is non-null, notify with
  // OnEntryCommitted() after |number_of_navigations| navigations.
  // Note: |js_injection_ready_observer| is owned by the caller and should be
  // valid until this class is destroyed.
  TestNavigationObserver(NavigationController* controller,
                         JsInjectionReadyObserver* js_injection_ready_observer,
                         int number_of_navigations);

  virtual ~TestNavigationObserver();

  // Run the UI message loop until |done_| becomes true.
  void WaitForObservation();

 protected:
  // Note: |js_injection_ready_observer| is owned by the caller and should be
  // valid until this class is destroyed. Subclasses using this constructor MUST
  // call RegisterAsObserver when a NavigationController becomes available.
  explicit TestNavigationObserver(
      JsInjectionReadyObserver* js_injection_ready_observer,
      int number_of_navigations);

  // Register this TestNavigationObserver as an observer of the |controller|.
  void RegisterAsObserver(NavigationController* controller);

 private:
  // NotificationObserver:
  virtual void Observe(int type, const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // If true the navigation has been committed.
  bool navigation_entry_committed_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  // Observer to take some action when the page is ready for javascript
  // injection.
  JsInjectionReadyObserver* js_injection_ready_observer_;

  // |done_| will get set when this object observes a TabStripModel event.
  bool done_;

  // |running_| will be true during WaitForObservation until |done_| is true.
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserver);
};

#endif  // CHROME_TEST_TEST_NAVIGATION_OBSERVER_H_
