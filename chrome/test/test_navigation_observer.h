// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_NAVIGATION_OBSERVER_H_
#define CHROME_TEST_TEST_NAVIGATION_OBSERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class NavigationController;
class RenderViewHost;

// For browser_tests, which run on the UI thread, run a second
// MessageLoop and quit when the navigation completes loading. For
// WebUI tests that need to inject javascript, construct with a
// JsInjectionReadyObserver and this class will call its
// OnJsInjectionReady() at the appropriate time.
class TestNavigationObserver : public NotificationObserver {
 public:
  class RVHOSendJS;

  // Interface to notify when JavaScript injection is possible.
  class JsInjectionReadyObserver {
   public:
    JsInjectionReadyObserver();
    virtual ~JsInjectionReadyObserver();

    // Called to indicate page entry committed and ready for JavaScript
    // injection.
    virtual void OnJsInjectionReady(RenderViewHost* render_view_host) = 0;
  };

  // Create and register a new TestNavigationObserver against the
  // |controller|. When |js_injection_ready_observer| is non-null, notify with
  // OnEntryCommitted() after |number_of_navigations| navigations.
  // Note: |js_injection_ready_observer| is owned by the caller and should be
  // valid until this class is destroyed.
  TestNavigationObserver(const NotificationSource& source,
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

  // Register this TestNavigationObserver as an observer of the |source|.
  void RegisterAsObserver(const NotificationSource& source);

 private:
  // NotificationObserver:
  virtual void Observe(int type, const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  // Observer to take some action when the page is ready for JavaScript
  // injection.
  JsInjectionReadyObserver* js_injection_ready_observer_;

  // |done_| will get set when this object observes a TabStripModel event.
  bool done_;

  // |running_| will be true during WaitForObservation until |done_| is true.
  bool running_;

  // |rvho_send_js_| will hold a RenderViewHostObserver subclass to allow
  // JavaScript injection at the appropriate time.
  scoped_ptr<RVHOSendJS> rvho_send_js_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserver);
};

#endif  // CHROME_TEST_TEST_NAVIGATION_OBSERVER_H_
