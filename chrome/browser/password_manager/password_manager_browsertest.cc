// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace {

class NavigationObserver : public content::NotificationObserver,
                           public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner),
        info_bar_shown_(false),
        infobar_service_(InfoBarService::FromWebContents(web_contents)) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                   content::Source<InfoBarService>(infobar_service_));
  }

  virtual ~NavigationObserver() {}

  void Wait() {
    message_loop_runner_->Run();
  }

  bool InfoBarWasShown() {
    return info_bar_shown_;
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    // Accept in the infobar.
    InfoBarDelegate* infobar = infobar_service_->GetInfoBarDelegateAt(0);
    ConfirmInfoBarDelegate* confirm_infobar =
        infobar->AsConfirmInfoBarDelegate();
    confirm_infobar->Accept();
    info_bar_shown_ = true;
  }

  // content::WebContentsObserver
  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame,
                             content::RenderViewHost* render_view_host) {
    message_loop_runner_->Quit();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool info_bar_shown_;
  content::NotificationRegistrar registrar_;
  InfoBarService* infobar_service_;
};

}  // namespace

class PasswordManagerBrowserTest : public InProcessBrowserTest {
 protected:
  content::WebContents* WebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderViewHost* RenderViewHost() {
    return WebContents()->GetRenderViewHost();
  }
};

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForXHRSubmit) {
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL("files/password/password_xhr_submit.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Verify that we show the save password prompt if a form returns false
  // in its onsubmit handler but instead logs in/navigates via XHR.
  // Note that calling 'submit()' on a form with javascript doesn't call
  // the onsubmit handler, so we click the submit button instead.
  NavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(observer.InfoBarWasShown());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForOtherXHR) {
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL("files/password/password_xhr_submit.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Verify that if random XHR navigation occurs, we don't try and save the
  // password.
  //
  // We may want to change this functionality in the future to account for
  // cases where the element that users click on isn't a submit button.
  NavigationObserver observer(WebContents());
  std::string fill_and_navigate =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "send_xhr()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_navigate));
  observer.Wait();
  EXPECT_FALSE(observer.InfoBarWasShown());
}
