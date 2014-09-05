// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/value_builder.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {
namespace {

class LocationBarControllerUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    active_script_override_.reset(new FeatureSwitch::ScopedOverride(
        FeatureSwitch::scripts_require_action(), true));

    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
  test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    TabHelper::CreateForWebContents(web_contents());
    // Create an ExtensionService so the LocationBarController can find its
    // extensions.
    CommandLine command_line(CommandLine::NO_PROGRAM);
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile))->CreateExtensionService(
            &command_line, base::FilePath(), false);
  }

  virtual void TearDown() OVERRIDE {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  int tab_id() {
    return SessionTabHelper::IdForTab(web_contents());
  }

  const Extension* AddExtension(bool has_page_actions,
                                const std::string& name) {
    DictionaryBuilder manifest;
    manifest.Set("name", name)
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("permissions", ListBuilder().Append("tabs"));
    if (has_page_actions) {
      manifest.Set("page_action", DictionaryBuilder()
              .Set("default_title", "Hello"));
    }
    scoped_refptr<const Extension> extension =
        ExtensionBuilder().SetManifest(manifest.Pass())
                          .SetID(crx_file::id_util::GenerateId(name))
                          .Build();
    extension_service_->AddExtension(extension.get());
    return extension.get();
  }

  ExtensionService* extension_service_;

 private:
#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

  // Since we also test that we show page actions for pending script requests,
  // we need to enable that feature.
  scoped_ptr<FeatureSwitch::ScopedOverride> active_script_override_;
};

// Test that the location bar gets the proper current actions.
TEST_F(LocationBarControllerUnitTest, LocationBarDisplaysPageActions) {
  // Load up two extensions, one with a page action and one without.
  const Extension* page_action = AddExtension(true, "page_actions");
  const Extension* no_action = AddExtension(false, "no_actions");

  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);
  LocationBarController* controller = tab_helper->location_bar_controller();
  ASSERT_TRUE(controller);

  // There should only be one action - the action for the extension with a
  // page action.
  std::vector<ExtensionAction*> current_actions =
      controller->GetCurrentActions();
  ASSERT_EQ(1u, current_actions.size());
  EXPECT_EQ(page_action->id(), current_actions[0]->extension_id());

  // If we request a script injection, then the location bar controller should
  // also show a page action for that extension.
  ActiveScriptController* active_script_controller =
      ActiveScriptController::GetForWebContents(web_contents());
  ASSERT_TRUE(active_script_controller);
  active_script_controller->RequestScriptInjectionForTesting(no_action,
                                                             base::Closure());
  current_actions = controller->GetCurrentActions();
  ASSERT_EQ(2u, current_actions.size());
  // Check that each extension is present in the vector.
  EXPECT_TRUE(current_actions[0]->extension_id() == no_action->id() ||
              current_actions[1]->extension_id() == no_action->id());
  EXPECT_TRUE(current_actions[0]->extension_id() == page_action->id() ||
              current_actions[1]->extension_id() == page_action->id());

  // If we request a script injection for an extension that already has a
  // page action, only one action should be visible.
  active_script_controller->RequestScriptInjectionForTesting(page_action,
                                                             base::Closure());
  current_actions = controller->GetCurrentActions();
  ASSERT_EQ(2u, current_actions.size());
  EXPECT_TRUE(current_actions[0]->extension_id() == no_action->id() ||
              current_actions[1]->extension_id() == no_action->id());
  EXPECT_TRUE(current_actions[0]->extension_id() == page_action->id() ||
              current_actions[1]->extension_id() == page_action->id());

  // Navigating away means that only page actions are shown again.
  NavigateAndCommit(GURL("http://google.com"));
  current_actions = controller->GetCurrentActions();
  ASSERT_EQ(1u, current_actions.size());
  EXPECT_EQ(page_action->id(), current_actions[0]->extension_id());
}

// Test that navigating clears all state in a page action.
TEST_F(LocationBarControllerUnitTest, NavigationClearsState) {
  const Extension* extension = AddExtension(true, "page_actions");

  NavigateAndCommit(GURL("http://www.google.com"));

  ExtensionAction& page_action =
      *ExtensionActionManager::Get(profile())->GetPageAction(*extension);
  page_action.SetTitle(tab_id(), "Goodbye");
  page_action.SetPopupUrl(tab_id(), extension->GetResourceURL("popup.html"));

  EXPECT_EQ("Goodbye", page_action.GetTitle(tab_id()));
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            page_action.GetPopupUrl(tab_id()));

  // Within-page navigation should keep the settings.
  NavigateAndCommit(GURL("http://www.google.com/#hash"));

  EXPECT_EQ("Goodbye", page_action.GetTitle(tab_id()));
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            page_action.GetPopupUrl(tab_id()));

  // Should discard the settings, and go back to the defaults.
  NavigateAndCommit(GURL("http://www.yahoo.com"));

  EXPECT_EQ("Hello", page_action.GetTitle(tab_id()));
  EXPECT_EQ(GURL(), page_action.GetPopupUrl(tab_id()));
}

}  // namespace
}  // namespace extensions
