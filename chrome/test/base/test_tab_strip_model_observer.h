// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_TEST_BASE_TEST_TAB_STRIP_MODEL_OBSERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/test_navigation_observer.h"

class TabStripModel;

// In order to support testing of print preview, we need to wait for the
// constrained window to block the current tab, and then observe notifications
// on the newly added tab's controller to wait for it to be loaded.
// To support tests registering javascript WebUI handlers, we need to inject
// the framework & registration javascript before the webui page loads by
// calling back through the TestTabStripModelObserver::LoadStartObserver when
// the new page starts loading.
class TestTabStripModelObserver : public TestNavigationObserver,
                                  public TabStripModelObserver {
 public:
  // Observe the |tab_strip_model|, which may not be NULL. If
  // |load_start_observer| is non-NULL, notify when the page load starts.
  TestTabStripModelObserver(
      TabStripModel* tab_strip_model,
      JsInjectionReadyObserver* js_injection_ready_observer);
  virtual ~TestTabStripModelObserver();

 private:
  // Callback to observer the print preview tab associated with |contents|.
  void ObservePrintPreviewTabContents(TabContentsWrapper* contents);

  // TabStripModelObserver:
  virtual void TabBlockedStateChanged(TabContentsWrapper* contents,
                                      int index) OVERRIDE;

  // |tab_strip_model_| is the object this observes. The constructor will
  // register this as an observer, and the destructor will remove the observer.
  TabStripModel* tab_strip_model_;

  DISALLOW_COPY_AND_ASSIGN(TestTabStripModelObserver);
};

#endif  // CHROME_TEST_BASE_TEST_TAB_STRIP_MODEL_OBSERVER_H_
