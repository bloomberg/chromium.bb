// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/ssl_status.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace test {

class WebsiteSettingsPopupViewTestApi {
 public:
  WebsiteSettingsPopupViewTestApi(gfx::NativeView parent,
                                  Profile* profile,
                                  content::WebContents* web_contents)
      : view_(nullptr),
        parent_(parent),
        profile_(profile),
        web_contents_(web_contents) {
    CreateView();
  }

  void CreateView() {
    if (view_)
      view_->GetWidget()->CloseNow();

    GURL url("http://www.example.com");
    SecurityStateModel::SecurityInfo security_info;
    views::View* anchor_view = nullptr;
    view_ = new WebsiteSettingsPopupView(anchor_view, parent_, profile_,
                                         web_contents_, url, security_info);
  }

  WebsiteSettingsPopupView* view() { return view_; }
  views::View* permissions_content() { return view_->permissions_content_; }

  PermissionSelectorView* GetPermissionSelectorAt(int index) {
    return static_cast<PermissionSelectorView*>(
        permissions_content()->child_at(index));
  }

  views::MenuButton* GetPermissionButtonAt(int index) {
    const int kButtonIndex = 2;  // Button should be the third child.
    views::View* view = GetPermissionSelectorAt(index)->child_at(kButtonIndex);
    EXPECT_EQ(views::MenuButton::kViewClassName, view->GetClassName());
    return static_cast<views::MenuButton*>(view);
  }

  // Simulates recreating the dialog with a new PermissionInfoList.
  void SetPermissionInfo(const PermissionInfoList& list) {
    for (const WebsiteSettingsPopupView::PermissionInfo& info : list)
      view_->presenter_->OnSitePermissionChanged(info.type, info.setting);
    CreateView();
  }

 private:
  WebsiteSettingsPopupView* view_;  // Weak. Owned by its Widget.

  // For recreating the view.
  gfx::NativeView parent_;
  Profile* profile_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupViewTestApi);
};

}  // namespace test

namespace {

// Helper class that wraps a TestingProfile and a TestWebContents for a test
// harness. Inspired by RenderViewHostTestHarness, but doesn't use inheritance
// so the helper can be composed with other helpers in the test harness.
class ScopedWebContentsTestHelper {
 public:
  ScopedWebContentsTestHelper() {
    web_contents_ = factory_.CreateWebContents(&profile_);
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;  // Weak. Owned by factory_.

  DISALLOW_COPY_AND_ASSIGN(ScopedWebContentsTestHelper);
};

class WebsiteSettingsPopupViewTest : public testing::Test {
 public:
  WebsiteSettingsPopupViewTest() {}

  // testing::Test:
  void SetUp() override {
    views::Widget::InitParams parent_params;
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(parent_params);

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    TabSpecificContentSettings::CreateForWebContents(web_contents);
    api_.reset(new test::WebsiteSettingsPopupViewTestApi(
        parent_window_->GetNativeView(), web_contents_helper_.profile(),
        web_contents));
  }

  void TearDown() override { parent_window_->CloseNow(); }

 protected:
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_;

  views::Widget* parent_window_ = nullptr;  // Weak. Owned by the NativeWidget.
  scoped_ptr<test::WebsiteSettingsPopupViewTestApi> api_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupViewTest);
};

}  // namespace

// Test UI construction and reconstruction via
// WebsiteSettingsPopupView::SetPermissionInfo().
TEST_F(WebsiteSettingsPopupViewTest, SetPermissionInfo) {
  PermissionInfoList list(1);
  list.back().type = CONTENT_SETTINGS_TYPE_GEOLOCATION;
  list.back().source = content_settings::SETTING_SOURCE_USER;

  EXPECT_EQ(0, api_->permissions_content()->child_count());

  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(1, api_->permissions_content()->child_count());

  PermissionSelectorView* selector = api_->GetPermissionSelectorAt(0);
  EXPECT_EQ(3, selector->child_count());

  // Verify labels match the settings on the PermissionInfoList.
  const int kLabelIndex = 1;
  EXPECT_EQ(views::Label::kViewClassName,
            selector->child_at(kLabelIndex)->GetClassName());
  views::Label* label =
      static_cast<views::Label*>(selector->child_at(kLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Location:"), label->text());
  EXPECT_EQ(base::ASCIIToUTF16("Allowed by you"),
            api_->GetPermissionButtonAt(0)->GetText());

  // Verify calling SetPermisisonInfo() directly updates the UI.
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(base::ASCIIToUTF16("Blocked by you"),
            api_->GetPermissionButtonAt(0)->GetText());

  // Simulate a user selection via the UI. Note this will also cover logic in
  // WebsiteSettings to update the pref.
  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->GetPermissionSelectorAt(0)->PermissionChanged(list.back());
  EXPECT_EQ(1, api_->permissions_content()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Allowed by you"),
            api_->GetPermissionButtonAt(0)->GetText());

  // Setting to the default via the UI should keep the button around.
  list.back().setting = CONTENT_SETTING_ASK;
  api_->GetPermissionSelectorAt(0)->PermissionChanged(list.back());
  EXPECT_EQ(1, api_->permissions_content()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Ask by you"),
            api_->GetPermissionButtonAt(0)->GetText());

  // However, since the setting is now default, recreating the dialog with those
  // settings should omit the permission from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(0, api_->permissions_content()->child_count());
}
