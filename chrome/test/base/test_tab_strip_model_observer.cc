// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_tab_strip_model_observer.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/common/notification_source.h"

TestTabStripModelObserver::TestTabStripModelObserver(
    TabStripModel* tab_strip_model,
    TestTabStripModelObserver::JsInjectionReadyObserver*
        js_injection_ready_observer)
    : TestNavigationObserver(js_injection_ready_observer, 1),
      tab_strip_model_(tab_strip_model) {
  tab_strip_model_->AddObserver(this);
}

TestTabStripModelObserver::~TestTabStripModelObserver() {
  tab_strip_model_->RemoveObserver(this);
}

void TestTabStripModelObserver::TabInsertedAt(
    TabContentsWrapper* contents, int index, bool foreground) {
  RegisterAsObserver(Source<NavigationController>(&contents->controller()));
}
