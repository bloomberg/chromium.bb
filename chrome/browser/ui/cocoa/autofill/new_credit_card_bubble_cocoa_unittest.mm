// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/new_credit_card_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest_mac.h"
using autofill::NewCreditCardBubbleView;
using autofill::NewCreditCardBubbleController;
using autofill::NewCreditCardBubbleCocoa;
using content::SiteInstance;
using content::WebContents;

namespace autofill {

class NewCreditCardBubbleCocoaUnitTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    site_instance_ = SiteInstance::Create(profile());
  }

  NewCreditCardBubbleController* CreateController(WebContents* contents) {
    return new NewCreditCardBubbleController(contents);
  }

  WebContents* AppendToTabStrip() {
    WebContents* web_contents = WebContents::Create(
        content::WebContents::CreateParams(profile(), site_instance_.get()));
    browser()->tab_strip_model()->AppendWebContents(
        web_contents, /*foreground=*/true);
    return web_contents;
 }

 private:
  scoped_refptr<SiteInstance> site_instance_;
};

TEST_F(NewCreditCardBubbleCocoaUnitTest, CloseDeletes) {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
  NSWindow* window = browser()->window()->GetNativeWindow();
  WebContents* web_contents = AppendToTabStrip();
  NewCreditCardBubbleController* controller = CreateController(web_contents);
  base::WeakPtr<NewCreditCardBubbleView> view =
      NewCreditCardBubbleView::Create(controller);
  autorelease_pool.Recycle();
  ASSERT_TRUE(view.get());

  // Now reach deep into |view| and pre-create the bubble controller...
  NewCreditCardBubbleCocoa* cocoa_view =
      static_cast<NewCreditCardBubbleCocoa*>(view.get());
  cocoa_view->CreateCocoaController(window);

  // ... so window animations can be disabled, guaranteeing prompt deallocation.
  InfoBubbleWindow* bubble_window = cocoa_view->GetInfoBubbleWindow();
  [bubble_window setAllowedAnimations:info_bubble::kAnimateNone];

  view->Show();
  autorelease_pool.Recycle();
  ASSERT_TRUE(view.get());

  view->Hide();
  autorelease_pool.Recycle();
  ASSERT_FALSE(view.get());

  // |controller| gets deleted via the bubble closing.
}

}  // autofill
