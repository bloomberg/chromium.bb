// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/chosen_object_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"

const char* kUrl = "http://www.example.com/index.html";

namespace test {

class PageInfoBubbleViewTestApi {
 public:
  PageInfoBubbleViewTestApi(gfx::NativeView parent,
                            Profile* profile,
                            content::WebContents* web_contents)
      : view_(nullptr),
        parent_(parent),
        profile_(profile),
        web_contents_(web_contents) {
    CreateView();
  }

  void CreateView() {
    if (view_) {
      view_->GetWidget()->CloseNow();
    }

    security_state::SecurityInfo security_info;
    views::View* anchor_view = nullptr;
    view_ = new PageInfoBubbleView(anchor_view, parent_, profile_,
                                   web_contents_, GURL(kUrl), security_info);
  }

  PageInfoBubbleView* view() { return view_; }
  views::View* permissions_view() { return view_->permissions_view_; }

  PermissionSelectorRow* GetPermissionSelectorAt(int index) {
    return view_->selector_rows_[index].get();
  }

  // Returns the permission label text of the |index|th permission selector row.
  // This function returns an empty string if the permission selector row's
  // |label_| element isn't actually a |views::Label|.
  base::string16 GetPermissionLabelTextAt(int index) {
    views::View* view = GetPermissionSelectorAt(index)->label_;
    if (view->GetClassName() == views::Label::kViewClassName) {
      return static_cast<views::Label*>(view)->text();
    }
    return base::string16();
  }

  base::string16 GetPermissionButtonTextAt(int index) {
    views::View* view = GetPermissionSelectorAt(index)->button();
    if (view->GetClassName() == views::MenuButton::kViewClassName) {
      return static_cast<views::MenuButton*>(view)->GetText();
    } else if (view->GetClassName() == views::Combobox::kViewClassName) {
      views::Combobox* combobox = static_cast<views::Combobox*>(view);
      return combobox->GetTextForRow(combobox->GetSelectedRow());
    } else {
      NOTREACHED() << "Unknown class " << view->GetClassName();
      return base::string16();
    }
  }

  // Simulates recreating the dialog with a new PermissionInfoList.
  void SetPermissionInfo(const PermissionInfoList& list) {
    for (const PageInfoBubbleView::PermissionInfo& info : list) {
      view_->presenter_->OnSitePermissionChanged(info.type, info.setting);
    }
    CreateView();
  }

 private:
  PageInfoBubbleView* view_;  // Weak. Owned by its Widget.

  // For recreating the view.
  gfx::NativeView parent_;
  Profile* profile_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewTestApi);
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

class PageInfoBubbleViewTest : public testing::Test {
 public:
  PageInfoBubbleViewTest() {}

  // testing::Test:
  void SetUp() override {
    views_helper_.test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
    views::Widget::InitParams parent_params;
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(parent_params);

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    TabSpecificContentSettings::CreateForWebContents(web_contents);
    api_.reset(new test::PageInfoBubbleViewTestApi(
        parent_window_->GetNativeView(), web_contents_helper_.profile(),
        web_contents));
  }

  void TearDown() override { parent_window_->CloseNow(); }

 protected:
  device::MockDeviceClient device_client_;
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_;

  views::Widget* parent_window_ = nullptr;  // Weak. Owned by the NativeWidget.
  std::unique_ptr<test::PageInfoBubbleViewTestApi> api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewTest);
};

}  // namespace

// TODO(ellyjones): re-enable this test for OSX.
// This test exercises PermissionSelectorRow in a way that it is not used in
// practice. In practice, every setting in PermissionSelectorRow starts off
// "set", so there is always one option checked in the resulting MenuModel. This
// test creates settings that are left at their defaults, leading to zero
// checked options, and checks that the text on the MenuButtons is right. Since
// the Comboboxes the MacViews version of this dialog uses don't have separate
// text, this test doesn't work.
#if defined(OS_MACOSX)
#define MAYBE_SetPermissionInfo DISABLED_SetPermissionInfo
#else
#define MAYBE_SetPermissionInfo SetPermissionInfo
#endif

// Each permission selector row is like this: [icon] [label] [selector]
constexpr int kViewsPerPermissionRow = 3;

// Test UI construction and reconstruction via
// PageInfoBubbleView::SetPermissionInfo().
TEST_F(PageInfoBubbleViewTest, MAYBE_SetPermissionInfo) {
  PermissionInfoList list(1);
  list.back().type = CONTENT_SETTINGS_TYPE_GEOLOCATION;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;
  list.back().setting = CONTENT_SETTING_DEFAULT;

  const int kExpectedChildren =
      kViewsPerPermissionRow *
      (ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ? 11 : 13);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());

  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());

  PermissionSelectorRow* selector = api_->GetPermissionSelectorAt(0);
  EXPECT_TRUE(selector);

  // Verify labels match the settings on the PermissionInfoList.
  EXPECT_EQ(base::ASCIIToUTF16("Location"), api_->GetPermissionLabelTextAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("Allow"), api_->GetPermissionButtonTextAt(0));

  // Verify calling SetPermisisonInfo() directly updates the UI.
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(base::ASCIIToUTF16("Block"), api_->GetPermissionButtonTextAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // PageInfo to update the pref.
  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->GetPermissionSelectorAt(0)->PermissionChanged(list.back());
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Allow"), api_->GetPermissionButtonTextAt(0));

  // Setting to the default via the UI should keep the button around.
  list.back().setting = CONTENT_SETTING_ASK;
  api_->GetPermissionSelectorAt(0)->PermissionChanged(list.back());
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), api_->GetPermissionButtonTextAt(0));

  // However, since the setting is now default, recreating the dialog with those
  // settings should omit the permission from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());
}

// Test UI construction and reconstruction with USB devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithUsbDevice) {
  const int kExpectedChildren =
      kViewsPerPermissionRow *
      (ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ? 11 : 13);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());

  const GURL origin = GURL(kUrl).GetOrigin();
  scoped_refptr<device::UsbDevice> device =
      new device::MockUsbDevice(0, 0, "Google", "Gizmo", "1234567890");
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store =
      UsbChooserContextFactory::GetForProfile(web_contents_helper_.profile());
  store->GrantDevicePermission(origin, origin, device->guid());

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->permissions_view()->child_count());

  ChosenObjectRow* object_view = static_cast<ChosenObjectRow*>(
      api_->permissions_view()->child_at(kExpectedChildren));
  EXPECT_EQ(3, object_view->child_count());

  const int kLabelIndex = 1;
  views::Label* label =
      static_cast<views::Label*>(object_view->child_at(kLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Gizmo"), label->text());

  const int kButtonIndex = 2;
  views::Button* button =
      static_cast<views::Button*>(object_view->child_at(kButtonIndex));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  views::ButtonListener* button_listener =
      static_cast<views::ButtonListener*>(object_view);
  button_listener->ButtonPressed(button, event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device));
}
