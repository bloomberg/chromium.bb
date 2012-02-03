// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_dialog.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"

namespace {

GURL GetSyncPromoURL() {
  return SyncPromoUI::GetSyncPromoURL(GURL("http://a/"), true, "");
}

void OpenLink(content::WebContents* source) {
  content::Referrer referrer(GURL(), WebKit::WebReferrerPolicyAlways);
  content::OpenURLParams params(GURL("http://b/"), referrer, NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, true);
  source->GetDelegate()->OpenURLFromTab(source, params);
}

class TestSyncPromoDialog : public SyncPromoDialog {
 public:
  TestSyncPromoDialog(Profile* profile, GURL url)
      : SyncPromoDialog(profile, url),
        on_load_close_(false),
        on_load_open_link_(false) {
  }

  void set_on_load_close(bool flag) { on_load_close_ = flag; }
  void set_on_load_open_link(bool flag) { on_load_open_link_ = flag; }

  virtual void OnLoadingStateChanged(content::WebContents* source) OVERRIDE {
    if (!source || source->IsLoading())
      return;
    if (on_load_close_) {
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&browser::CloseHtmlDialog, window()));
    } else if (on_load_open_link_) {
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&OpenLink, source));
    }
  }

 private:
  bool on_load_close_;
  bool on_load_open_link_;
};

typedef InProcessBrowserTest SyncPromoDialogBrowserTest;

// Test that closing the dialog window spawns a new browser and end the modal
// session.
IN_PROC_BROWSER_TEST_F(SyncPromoDialogBrowserTest, Close) {
  TestSyncPromoDialog dialog(browser()->profile(), GetSyncPromoURL());
  SyncPromoUI::DidShowSyncPromoAtStartup(browser()->profile());
  dialog.set_on_load_close(true);
  dialog.ShowDialog();

  EXPECT_FALSE(dialog.spawned_browser());
  EXPECT_FALSE(dialog.sync_promo_was_closed());
}

// Test that clicking a link in the dialog window spawns a new browser window
// and ends the modal session.
IN_PROC_BROWSER_TEST_F(SyncPromoDialogBrowserTest, OpenLink) {
  TestSyncPromoDialog dialog(browser()->profile(), GetSyncPromoURL());
  SyncPromoUI::DidShowSyncPromoAtStartup(browser()->profile());
  dialog.set_on_load_open_link(true);
  dialog.ShowDialog();

  EXPECT_TRUE(dialog.spawned_browser());
  EXPECT_FALSE(dialog.sync_promo_was_closed());
}

}  // namespace
