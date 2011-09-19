// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error_service.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

// An error that has a bubble view.
class BubbleViewError : public GlobalError {
 public:
  BubbleViewError() : bubble_view_close_count_(0) { }

  int bubble_view_close_count() { return bubble_view_close_count_; }

  bool HasBadge() OVERRIDE { return false; }
  virtual int GetBadgeResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }

  virtual bool HasMenuItem() OVERRIDE { return false; }
  virtual int MenuItemCommandID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual string16 MenuItemLabel() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual int MenuItemIconResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE { ADD_FAILURE(); }

  virtual bool HasBubbleView() OVERRIDE { return true; }
  virtual string16 GetBubbleViewTitle() OVERRIDE {
    return string16();
  }
  virtual string16 GetBubbleViewMessage() OVERRIDE {
    return string16();
  }
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE {
    return string16();
  }
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE {
    return string16();
  }
  virtual void BubbleViewDidClose() OVERRIDE {}
  virtual void BubbleViewAcceptButtonPressed() OVERRIDE {}
  virtual void BubbleViewCancelButtonPressed() OVERRIDE {}

 private:
  int bubble_view_close_count_;

  DISALLOW_COPY_AND_ASSIGN(BubbleViewError);
};

} // namespace

class GlobalErrorServiceBrowserTest : public InProcessBrowserTest {
};

// Test that showing a error with a bubble view works.
IN_PROC_BROWSER_TEST_F(GlobalErrorServiceBrowserTest, ShowBubbleView) {
  // This will be deleted by the GlobalErrorService.
  BubbleViewError* error = new BubbleViewError;

  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  service->AddGlobalError(error);

  EXPECT_EQ(error, service->GetFirstGlobalErrorWithBubbleView());
  EXPECT_FALSE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());

  // Creating a second browser window should show the bubble view.
  CreateBrowser(browser()->profile());
  EXPECT_EQ(NULL, service->GetFirstGlobalErrorWithBubbleView());
  EXPECT_TRUE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());
}
