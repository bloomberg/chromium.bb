// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_tab_strip_model_observer.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"

using content::NavigationController;

TestTabStripModelObserver::TestTabStripModelObserver(
    TabStripModel* tab_strip_model,
    JsInjectionReadyObserver* js_injection_ready_observer)
    : TestNavigationObserver(js_injection_ready_observer, 1),
      tab_strip_model_(tab_strip_model) {
  tab_strip_model_->AddObserver(this);
}

TestTabStripModelObserver::~TestTabStripModelObserver() {
  tab_strip_model_->RemoveObserver(this);
}

void TestTabStripModelObserver::TabBlockedStateChanged(
    TabContentsWrapper* contents, int index) {
  // Need to do this later - the print preview tab has not been created yet.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TestTabStripModelObserver::ObservePrintPreviewTabContents,
                 base::Unretained(this),
                 contents));
}

void TestTabStripModelObserver::ObservePrintPreviewTabContents(
    TabContentsWrapper* contents) {
  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  if (tab_controller) {
    TabContentsWrapper* preview_tab =
        tab_controller->GetPrintPreviewForTab(contents);
    if (preview_tab) {
      RegisterAsObserver(content::Source<NavigationController>(
          &preview_tab->web_contents()->GetController()));
    }
  }
}
