// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_TEST_TEST_TAB_STRIP_MODEL_OBSERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class TabStripModel;

// In order to support testing of print preview, we need to wait for the tab to
// be inserted, and then observe notifications on the newly added tab's
// controller to wait for it to be loaded. To support tests registering
// javascript WebUI handlers, we need to inject the framework & registration
// javascript before the webui page loads by calling back through the
// TestTabStripModelObserver::LoadStartObserver when the new page starts
// loading.
class TestTabStripModelObserver : public TabStripModelObserver,
                                  public NotificationObserver {
 public:
  class LoadStartObserver {
   public:
    LoadStartObserver();
    virtual ~LoadStartObserver();

    // Called to indicate page load starting.
    virtual void OnLoadStart() = 0;
  };

  // Observe the |tab_strip_model|, which may not be NULL. If
  // |load_start_observer| is non-NULL, notify when the page load starts.
  TestTabStripModelObserver(TabStripModel* tab_strip_model,
                            LoadStartObserver* load_start_observer);
  virtual ~TestTabStripModelObserver();

  // Run the UI message loop until |done_| becomes true.
  void WaitForObservation();

 private:
  // TabStripModelObserver:
  virtual void TabInsertedAt(TabContentsWrapper* contents, int index,
                             bool foreground) OVERRIDE;

  // NotificationObserver:
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  // |tab_strip_model_| is the object this observes. The constructor will
  // register this as an observer, and the destructor will remove the observer.
  TabStripModel* tab_strip_model_;

  // Observer to take some action when the page load starts.
  LoadStartObserver* load_start_observer_;

  // |done_| will get set when this object observes a TabStripModel event.
  bool done_;

  // |running_| will be true during WaitForObservation until |done_| is true.
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(TestTabStripModelObserver);
};

#endif  // CHROME_TEST_TEST_TAB_STRIP_MODEL_OBSERVER_H_
