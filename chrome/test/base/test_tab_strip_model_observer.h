// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_TEST_BASE_TEST_TAB_STRIP_MODEL_OBSERVER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/test/test_navigation_observer.h"

class TabStripModel;

namespace content {
class JsInjectionReadyObserver;
}

// In order to support testing of print preview, we need to wait for the
// constrained window to block the current tab, and then observe notifications
// on the newly added tab's controller to wait for it to be loaded.
// To support tests registering javascript WebUI handlers, we need to inject
// the framework & registration javascript before the webui page loads by
// calling back through the TestTabStripModelObserver::LoadStartObserver when
// the new page starts loading.
class TestTabStripModelObserver : public content::TestNavigationObserver,
                                  public TabStripModelObserver {
 public:
  // Observe the |tab_strip_model|, which may not be NULL. If
  // |load_start_observer| is non-NULL, notify when the page load starts.
  TestTabStripModelObserver(
      TabStripModel* tab_strip_model,
      content::JsInjectionReadyObserver* js_injection_ready_observer);
  virtual ~TestTabStripModelObserver();

 private:
  class RenderViewHostInitializedObserver;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Callback to observer the print preview dialog associated with |contents|.
  void ObservePrintPreviewDialog(content::WebContents* contents);

  // TabStripModelObserver:
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int index) OVERRIDE;

  // |tab_strip_model_| is the object this observes. The constructor will
  // register this as an observer, and the destructor will remove the observer.
  TabStripModel* tab_strip_model_;

  // RenderViewHost watched for JS injection.
  scoped_ptr<RenderViewHostInitializedObserver> rvh_observer_;

  content::JsInjectionReadyObserver* injection_observer_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestTabStripModelObserver);
};

#endif  // CHROME_TEST_BASE_TEST_TAB_STRIP_MODEL_OBSERVER_H_
