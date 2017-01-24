// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/web_state/blocked_popup_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

// Test fixture for BlockedPopupTabHelper class.
class BlockedPopupTabHelperTest : public PlatformTest {
 protected:
  BlockedPopupTabHelperTest() : web_state_(new web::TestWebState()) {
    web_state_->SetNavigationManager(
        base::MakeUnique<web::TestNavigationManager>());

    BlockedPopupTabHelper::CreateForWebState(web_state_.get());
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
  }

 protected:
  // Expose ivars for testing.
  bool IsObservingSources() {
    BlockedPopupTabHelper* helper =
        BlockedPopupTabHelper::FromWebState(web_state_.get());
    return helper->scoped_observer_.IsObservingSources();
  }

  std::unique_ptr<web::TestWebState> web_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlockedPopupTabHelperTest);
};

// Tests that an infobar is added to the infobar manager when
// BlockedPopupTabHelper::HandlePopup() is called.
TEST_F(BlockedPopupTabHelperTest, ShowAndDismissInfoBar) {
  const GURL test_url("https://popups.example.com");
  web::BlockedPopupInfo popup_info(test_url, web::Referrer(), nil, nil);

  // Check that there are no infobars showing and no registered observers.
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_.get());
  EXPECT_EQ(0U, infobar_manager->infobar_count());
  EXPECT_FALSE(IsObservingSources());

  // Call |HandlePopup| to show an infobar.
  BlockedPopupTabHelper* tab_helper =
      BlockedPopupTabHelper::FromWebState(web_state_.get());
  tab_helper->HandlePopup(popup_info);
  ASSERT_EQ(1U, infobar_manager->infobar_count());
  EXPECT_TRUE(IsObservingSources());

  // Dismiss the infobar and check that the tab helper no longer has any
  // registered observers.
  infobar_manager->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_manager->infobar_count());
  EXPECT_FALSE(IsObservingSources());
}
