// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_html_dialog_observer.h"

#include "chrome/common/chrome_notification_types.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "chrome/test/base/ui_test_utils.h"

TestHtmlDialogObserver::TestHtmlDialogObserver()
    : web_ui_(NULL), done_(false), running_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_HTML_DIALOG_SHOWN,
                 NotificationService::AllSources());
}

TestHtmlDialogObserver::~TestHtmlDialogObserver() {
}

void TestHtmlDialogObserver::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_HTML_DIALOG_SHOWN:
      web_ui_ = Source<WebUI>(source).ptr();
      registrar_.Remove(this, chrome::NOTIFICATION_HTML_DIALOG_SHOWN,
                        NotificationService::AllSources());
      // Wait for navigation on the new WebUI instance to complete. This depends
      // on receiving the notification of the HtmlDialog being shown before the
      // NavigationController finishes loading. The HtmlDialog notification is
      // issued from html_dialog_ui.cc on RenderView creation which results from
      // the call to render_manager_.Navigate in the method
      // TabContents::NavigateToEntry. The new RenderView is later told to
      // navigate in this method, ensuring that this is not a race condition.
      registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                     Source<NavigationController>(
                         &web_ui_->tab_contents()->controller()));
      break;
    case content::NOTIFICATION_LOAD_STOP:
      DCHECK(web_ui_);
      registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                        Source<NavigationController>(
                            &web_ui_->tab_contents()->controller()));
      done_ = true;
      // If the message loop is running stop it.
      if (running_) {
        running_ = false;
        MessageLoopForUI::current()->Quit();
      }
      break;
    default:
      NOTREACHED();
  };
}

WebUI* TestHtmlDialogObserver::GetWebUI() {
  if (!done_) {
    EXPECT_FALSE(running_);
    running_ = true;
    ui_test_utils::RunMessageLoop();
  }
  return web_ui_;
}
