// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/web_app_left_header_view_ash.h"

#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "components/toolbar/test_toolbar_model.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "ui/aura/window.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

class WebAppLeftHeaderViewTest : public TestWithBrowserView {
 public:
  WebAppLeftHeaderViewTest()
      : TestWithBrowserView(Browser::TYPE_TABBED,
                            chrome::HOST_DESKTOP_TYPE_ASH,
                            true),
        frame_view_(nullptr),
        test_toolbar_model_(nullptr) {}
  ~WebAppLeftHeaderViewTest() override {}

  // TestWithBrowserView override:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableWebAppFrame);

    TestWithBrowserView::SetUp();

    // Setup a fake toolbar to enable testing.
    test_toolbar_model_ = new TestToolbarModel();
    scoped_ptr<ToolbarModel> toolbar_model(test_toolbar_model_);
    browser()->swap_toolbar_models(&toolbar_model);
    test_toolbar_model_->set_icon(gfx::VectorIconId::LOCATION_BAR_HTTP);

    AddTab(browser(), GURL("about:blank"));
    NavigateAndCommitActiveTab(GURL("http://www.google.com"));
    browser()->window()->Show();

    views::Widget* widget = browser_view()->GetWidget();
    frame_view_ = static_cast<BrowserNonClientFrameViewAsh*>(
                      widget->non_client_view()->frame_view());
  }

  Browser* CreateBrowser(Profile* profile,
                         Browser::Type browser_type,
                         bool hosted_app,
                         chrome::HostDesktopType host_desktop_type,
                         BrowserWindow* browser_window) override {
    RegisterExtension(profile);

    Browser::CreateParams params(profile);
    params = Browser::CreateParams::CreateForApp(
        "_crx_abc", false /* trusted_source */, gfx::Rect(), profile);
    params.window = browser_window;
    return new Browser(params);
  }

 protected:
  // Owned by the browser view.
  BrowserNonClientFrameViewAsh* frame_view_;

  // Owned by the browser.
  TestToolbarModel* test_toolbar_model_;

 private:
  void RegisterExtension(Profile* profile) {
    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName, "Test");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetString(extensions::manifest_keys::kLaunchWebURL,
        "http://www.google.com");

    std::string error;
    scoped_refptr<extensions::Extension> extension(
        extensions::Extension::Create(
            base::FilePath(), extensions::Manifest::UNPACKED, manifest,
            extensions::Extension::FROM_BOOKMARK, "abc", &error));

    ASSERT_TRUE(extension.get()) << error;

    extensions::ExtensionRegistry::Get(profile)->
        AddEnabled(extension);
  }

  DISALLOW_COPY_AND_ASSIGN(WebAppLeftHeaderViewTest);
};

TEST_F(WebAppLeftHeaderViewTest, BackButton) {
  WebAppLeftHeaderView* view = frame_view_->web_app_left_header_view_;

  // The left header view should not be null or our test is broken.
  ASSERT_TRUE(view);

  // The back button should be inactive until a navigate happens.
  EXPECT_EQ(views::Button::STATE_DISABLED, view->back_button_->state());

  NavigateAndCommitActiveTab(GURL("www2.google.com"));

  // The back button should be active now that a navigation happened.
  EXPECT_EQ(views::Button::STATE_NORMAL, view->back_button_->state());
}

TEST_F(WebAppLeftHeaderViewTest, LocationIcon) {
  WebAppLeftHeaderView* view = frame_view_->web_app_left_header_view_;

  // The left header view should not be null or our test is broken.
  ASSERT_TRUE(view);

  // The location icon should be non-secure one.
  EXPECT_EQ(gfx::VectorIconId::LOCATION_BAR_HTTP,
            view->location_icon_->icon_image_id());

  test_toolbar_model_->set_icon(gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID);
  NavigateAndCommitActiveTab(GURL("https://secure.google.com"));

  // The location icon should now be the secure one.
  EXPECT_EQ(gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID,
            view->location_icon_->icon_image_id());
}
