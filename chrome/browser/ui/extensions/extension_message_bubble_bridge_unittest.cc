// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/grit/components_scaled_resources.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

namespace {

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return base::MakeUnique<extensions::ExtensionWebUIOverrideRegistrar>(context);
}

std::unique_ptr<KeyedService> BuildToolbarModel(
    content::BrowserContext* context) {
  return base::MakeUnique<ToolbarActionsModel>(
      Profile::FromBrowserContext(context),
      extensions::ExtensionPrefs::Get(context));
}

}  // namespace

class ExtensionMessageBubbleBridgeUnitTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  ExtensionMessageBubbleBridgeUnitTest() {}
  ~ExtensionMessageBubbleBridgeUnitTest() override {}
  Browser* browser() { return browser_.get(); }

 private:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    browser_window_.reset(new TestBrowserWindow());
    Browser::CreateParams params(profile());
    params.type = Browser::TYPE_TABBED;
    params.window = browser_window_.get();
    browser_.reset(new Browser(params));

    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()
        ->SetTestingFactory(browser()->profile(), &BuildOverrideRegistrar);
    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(
        browser()->profile());
    ToolbarActionsModelFactory::GetInstance()->SetTestingFactory(
        browser()->profile(), &BuildToolbarModel);
  }

  void TearDown() override {
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestWithInstall::TearDown();
  }

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBridgeUnitTest);
};

TEST_F(ExtensionMessageBubbleBridgeUnitTest,
       TestGetExtraViewInfoMethodWithNormalSettingsOverrideExtension) {
  base::FilePath path(data_dir().AppendASCII("api_test/override/newtab/"));
  EXPECT_NE(nullptr, PackAndInstallCRX(path, INSTALL_NEW));

  std::unique_ptr<extensions::ExtensionMessageBubbleController>
      ntp_bubble_controller(new extensions::ExtensionMessageBubbleController(
          new extensions::NtpOverriddenBubbleDelegate(browser()->profile()),
          browser()));

  ASSERT_EQ(1U, ntp_bubble_controller->GetExtensionList().size());

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge(
      new ExtensionMessageBubbleBridge(std::move(ntp_bubble_controller)));

  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = bridge->GetExtraViewInfo();

  EXPECT_EQ(gfx::VectorIconId::VECTOR_ICON_NONE, extra_view_info->resource_id);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_LEARN_MORE), extra_view_info->text);
  EXPECT_EQ(true, extra_view_info->is_text_linked);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS),
      bridge->GetActionButtonText());
}

TEST_F(ExtensionMessageBubbleBridgeUnitTest,
       TestGetExtraViewInfoMethodWithPolicyInstalledSettingsOverrideExtension) {
  base::FilePath path(data_dir().AppendASCII("api_test/override/newtab/"));
  EXPECT_NE(nullptr,
            PackAndInstallCRX(path, extensions::Manifest::EXTERNAL_POLICY,
                              INSTALL_NEW));

  std::unique_ptr<extensions::ExtensionMessageBubbleController>
      ntp_bubble_controller(new extensions::ExtensionMessageBubbleController(
          new extensions::NtpOverriddenBubbleDelegate(browser()->profile()),
          browser()));

  ASSERT_EQ(1U, ntp_bubble_controller->GetExtensionList().size());

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge(
      new ExtensionMessageBubbleBridge(std::move(ntp_bubble_controller)));

  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = bridge->GetExtraViewInfo();

  extra_view_info = bridge->GetExtraViewInfo();

  EXPECT_EQ(gfx::VectorIconId::BUSINESS, extra_view_info->resource_id);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN),
            extra_view_info->text);
  EXPECT_EQ(false, extra_view_info->is_text_linked);

  EXPECT_EQ(base::string16(), bridge->GetActionButtonText());
}
