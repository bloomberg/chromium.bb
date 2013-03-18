// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_tab_strip_model_observer.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/js_injection_ready_observer.h"

class TestTabStripModelObserver::RenderViewHostInitializedObserver
    : public content::RenderViewHostObserver {
 public:
  RenderViewHostInitializedObserver(content::RenderViewHost* render_view_host,
                                    content::JsInjectionReadyObserver* observer)
      : content::RenderViewHostObserver(render_view_host),
        injection_observer_(observer) {
  }

  // content::RenderViewHostObserver:
  virtual void RenderViewHostInitialized() OVERRIDE {
    injection_observer_->OnJsInjectionReady(render_view_host());
  }

 private:
  content::JsInjectionReadyObserver* injection_observer_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostInitializedObserver);
};

TestTabStripModelObserver::TestTabStripModelObserver(
    TabStripModel* tab_strip_model,
    content::JsInjectionReadyObserver* js_injection_ready_observer)
    : TestNavigationObserver(1),
      tab_strip_model_(tab_strip_model),
      injection_observer_(js_injection_ready_observer) {
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
                 content::NotificationService::AllSources());

  tab_strip_model_->AddObserver(this);
}

TestTabStripModelObserver::~TestTabStripModelObserver() {
  tab_strip_model_->RemoveObserver(this);
}

void TestTabStripModelObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CREATED) {
    rvh_observer_.reset(
        new RenderViewHostInitializedObserver(
            content::Source<content::RenderViewHost>(source).ptr(),
            injection_observer_));
  } else {
    content::TestNavigationObserver::Observe(type, source, details);
  }
}

void TestTabStripModelObserver::TabBlockedStateChanged(
    content::WebContents* contents, int index) {
  // Need to do this later - the print preview dialog has not been created yet.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TestTabStripModelObserver::ObservePrintPreviewDialog,
                 base::Unretained(this),
                 contents));
}

void TestTabStripModelObserver::ObservePrintPreviewDialog(
    content::WebContents* contents) {
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return;
  content::WebContents* preview_dialog =
      dialog_controller->GetPrintPreviewForContents(contents);
  if (!preview_dialog)
    return;
  RegisterAsObserver(content::Source<content::NavigationController>(
      &preview_dialog->GetController()));
}
