// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

using content::WebContents;

namespace extensions {
namespace {

const char kEmptyImageDataError[] =
    "The imageData property must contain an ImageData object or dictionary "
    "of ImageData objects.";
const char kEmptyPathError[] = "The path property must not be empty.";

// Views implementation of browser action button will return icon whose
// background will be set.
gfx::ImageSkia AddBackgroundForViews(const gfx::ImageSkia& icon) {
#if defined(TOOLKIT_VIEWS)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia bg = *rb.GetImageSkiaNamed(IDR_BROWSER_ACTION);
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(bg, icon);
#else
  return icon;
#endif
}

bool ImagesAreEqualAtScale(const gfx::ImageSkia& i1,
                           const gfx::ImageSkia& i2,
                           float scale) {
  SkBitmap bitmap1 = i1.GetRepresentation(scale).sk_bitmap();
  SkBitmap bitmap2 = i2.GetRepresentation(scale).sk_bitmap();
  return gfx::BitmapsAreEqual(bitmap1, bitmap2);
}

class BrowserActionApiTest : public ExtensionApiTest {
 public:
  BrowserActionApiTest() {}
  virtual ~BrowserActionApiTest() {}

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }

  bool OpenPopup(int index) {
    ResultCatcher catcher;
    content::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    GetBrowserActionsBar().Press(index);
    popup_observer.Wait();
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
    return GetBrowserActionsBar().HasPopup();
  }

  ExtensionAction* GetBrowserAction(const Extension& extension) {
    return ExtensionActionManager::Get(browser()->profile())->
        GetBrowserAction(extension);
  }
};

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("browser_action/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Test that we received the changes.
  ExtensionAction* action = GetBrowserAction(*extension);
  ASSERT_EQ("Modified", action->GetTitle(ExtensionAction::kDefaultTabId));
  ASSERT_EQ("badge", action->GetBadgeText(ExtensionAction::kDefaultTabId));
  ASSERT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  // Simulate the browser action being clicked.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  ExtensionToolbarModel* toolbar_model = ExtensionToolbarModel::Get(
      browser()->profile());
  toolbar_model->ExecuteBrowserAction(extension, browser(), NULL, true);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action/no_icon")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

#if defined (OS_MACOSX)
  // We need this on mac so we don't loose 2x representations from browser icon
  // in transformations gfx::ImageSkia -> NSImage -> gfx::ImageSkia.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);
#endif

  // We should not be creating icons asynchronously, so we don't need an
  // observer.
  ExtensionActionIconFactory icon_factory(
      profile(),
      extension,
      GetBrowserAction(*extension),
      NULL);
  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());
  EXPECT_TRUE(GetBrowserActionsBar().HasIcon(0));

  gfx::Image action_icon = icon_factory.GetIcon(0);
  uint32_t action_icon_last_id = action_icon.ToSkBitmap()->getGenerationID();

  // Let's check that |GetIcon| doesn't always return bitmap with new id.
  ASSERT_EQ(action_icon_last_id,
            icon_factory.GetIcon(0).ToSkBitmap()->getGenerationID());

  uint32_t action_icon_current_id = 0;

  ResultCatcher catcher;

  // Tell the extension to update the icon using ImageData object.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(2.0f));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon.ToImageSkia()),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            1.0f));

  // Tell the extension to update the icon using path.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_FALSE(
      action_icon.ToImageSkia()->HasRepresentation(2.0f));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon.ToImageSkia()),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            1.0f));

  // Tell the extension to update the icon using dictionary of ImageData
  // objects.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(2.0f));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon.ToImageSkia()),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            1.0f));

  // Tell the extension to update the icon using dictionary of paths.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(2.0f));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon.ToImageSkia()),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            1.0f));

  // Tell the extension to update the icon using dictionary of ImageData
  // objects, but setting only size 19.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(2.0f));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon.ToImageSkia()),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            1.0f));

  // Tell the extension to update the icon using dictionary of paths, but
  // setting only size 19.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(2.0f));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon.ToImageSkia()),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            1.0f));

  // Tell the extension to update the icon using dictionary of ImageData
  // objects, but setting only size 38.
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  action_icon = icon_factory.GetIcon(0);

  const gfx::ImageSkia* action_icon_skia = action_icon.ToImageSkia();

  EXPECT_FALSE(action_icon_skia->HasRepresentation(1.0f));
  EXPECT_TRUE(action_icon_skia->HasRepresentation(2.0f));

  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *action_icon.ToSkBitmap(),
      action_icon_skia->GetRepresentation(2.0f).sk_bitmap()));

  EXPECT_TRUE(
      ImagesAreEqualAtScale(AddBackgroundForViews(*action_icon_skia),
                            *GetBrowserActionsBar().GetIcon(0).ToImageSkia(),
                            2.0f));

  // Try setting icon with empty dictionary of ImageData objects.
  GetBrowserActionsBar().Press(0);
  ASSERT_FALSE(catcher.GetNextResult());
  EXPECT_EQ(kEmptyImageDataError, catcher.message());

  // Try setting icon with empty dictionary of path objects.
  GetBrowserActionsBar().Press(0);
  ASSERT_FALSE(catcher.GetNextResult());
  EXPECT_EQ(kEmptyPathError, catcher.message());
}

// This test is flaky as per http://crbug.com/74557.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest,
                       DISABLED_TabSpecificBrowserActionState) {
  ASSERT_TRUE(RunExtensionTest("browser_action/tab_specific_state")) <<
      message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar and that it has an icon.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());
  EXPECT_TRUE(GetBrowserActionsBar().HasIcon(0));

  // Execute the action, its title should change.
  ResultCatcher catcher;
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_EQ("Showing icon 2", GetBrowserActionsBar().GetTooltip(0));

  // Open a new tab, the title should go back.
  chrome::NewTab(browser());
  EXPECT_EQ("hi!", GetBrowserActionsBar().GetTooltip(0));

  // Go back to first tab, changed title should reappear.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ("Showing icon 2", GetBrowserActionsBar().GetTooltip(0));

  // Reload that tab, default title should come back.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_EQ("hi!", GetBrowserActionsBar().GetTooltip(0));
}

// http://code.google.com/p/chromium/issues/detail?id=70829
// Mac used to be ok, but then mac 10.5 started failing too. =(
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, DISABLED_BrowserActionPopup) {
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("browser_action/popup")));
  BrowserActionTestUtil actions_bar = GetBrowserActionsBar();
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // The extension's popup's size grows by |growFactor| each click.
  const int growFactor = 500;
  gfx::Size minSize = BrowserActionTestUtil::GetMinPopupSize();
  gfx::Size middleSize = gfx::Size(growFactor, growFactor);
  gfx::Size maxSize = BrowserActionTestUtil::GetMaxPopupSize();

  // Ensure that two clicks will exceed the maximum allowed size.
  ASSERT_GT(minSize.height() + growFactor * 2, maxSize.height());
  ASSERT_GT(minSize.width() + growFactor * 2, maxSize.width());

  // Simulate a click on the browser action and verify the size of the resulting
  // popup.  The first one tries to be 0x0, so it should be the min values.
  ASSERT_TRUE(OpenPopup(0));
  EXPECT_EQ(minSize, actions_bar.GetPopupBounds().size());
  EXPECT_TRUE(actions_bar.HidePopup());

  ASSERT_TRUE(OpenPopup(0));
  EXPECT_EQ(middleSize, actions_bar.GetPopupBounds().size());
  EXPECT_TRUE(actions_bar.HidePopup());

  // One more time, but this time it should be constrained by the max values.
  ASSERT_TRUE(OpenPopup(0));
  EXPECT_EQ(maxSize, actions_bar.GetPopupBounds().size());
  EXPECT_TRUE(actions_bar.HidePopup());
}

// Test that calling chrome.browserAction.setPopup() can enable and change
// a popup.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, BrowserActionAddPopup) {
  ASSERT_TRUE(RunExtensionTest("browser_action/add_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  ExtensionAction* browser_action = GetBrowserAction(*extension);
  ASSERT_TRUE(browser_action)
      << "Browser action test extension should have a browser action.";

  ASSERT_FALSE(browser_action->HasPopup(tab_id));
  ASSERT_FALSE(browser_action->HasPopup(ExtensionAction::kDefaultTabId));

  // Simulate a click on the browser action icon.  The onClicked handler
  // will add a popup.
  {
    ResultCatcher catcher;
    GetBrowserActionsBar().Press(0);
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // The call to setPopup in background.html set a tab id, so the
  // current tab's setting should have changed, but the default setting
  // should not have changed.
  ASSERT_TRUE(browser_action->HasPopup(tab_id))
      << "Clicking on the browser action should have caused a popup to "
      << "be added.";
  ASSERT_FALSE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Clicking on the browser action should not have set a default "
      << "popup.";

  ASSERT_STREQ("/a_popup.html",
               browser_action->GetPopupUrl(tab_id).path().c_str());

  // Now change the popup from a_popup.html to another_popup.html by loading
  // a page which removes the popup using chrome.browserAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("change_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // The call to setPopup in change_popup.html did not use a tab id,
  // so the default setting should have changed as well as the current tab.
  ASSERT_TRUE(browser_action->HasPopup(tab_id));
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ("/another_popup.html",
               browser_action->GetPopupUrl(tab_id).path().c_str());
}

// Test that calling chrome.browserAction.setPopup() can remove a popup.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, BrowserActionRemovePopup) {
  // Load the extension, which has a browser action with a default popup.
  ASSERT_TRUE(RunExtensionTest("browser_action/remove_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  ExtensionAction* browser_action = GetBrowserAction(*extension);
  ASSERT_TRUE(browser_action)
      << "Browser action test extension should have a browser action.";

  ASSERT_TRUE(browser_action->HasPopup(tab_id))
      << "Expect a browser action popup before the test removes it.";
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Expect a browser action popup is the default for all tabs.";

  // Load a page which removes the popup using chrome.browserAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("remove_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ASSERT_FALSE(browser_action->HasPopup(tab_id))
      << "Browser action popup should have been removed.";
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Browser action popup default should not be changed by setting "
      << "a specific tab id.";
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, IncognitoBasic) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("browser_action/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Open an incognito window and test that the browser action isn't there by
  // default.
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser =
      new Browser(Browser::CreateParams(incognito_profile,
                                        browser()->host_desktop_type()));

  ASSERT_EQ(0,
            BrowserActionTestUtil(incognito_browser).NumberOfBrowserActions());

  // Now enable the extension in incognito mode, and test that the browser
  // action shows up. Note that we don't update the existing window at the
  // moment, so we just create a new one.
  extensions::ExtensionPrefs::Get(browser()->profile())
      ->SetIsIncognitoEnabled(extension->id(), true);

  chrome::CloseWindow(incognito_browser);
  incognito_browser =
      new Browser(Browser::CreateParams(incognito_profile,
                                        browser()->host_desktop_type()));
  ASSERT_EQ(1,
            BrowserActionTestUtil(incognito_browser).NumberOfBrowserActions());

  // TODO(mpcomplete): simulate a click and have it do the right thing in
  // incognito.
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, IncognitoDragging) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

  // The tooltips for each respective browser action.
  const char kTooltipA[] = "Alpha";
  const char kTooltipB[] = "Beta";
  const char kTooltipC[] = "Gamma";

  const size_t size_before = service->extensions()->size();

  base::FilePath test_dir = test_data_dir_.AppendASCII("browser_action");
  const Extension* extension_a = InstallExtension(
      test_dir.AppendASCII("empty_browser_action_alpha.crx"), 1);
  const Extension* extension_b = InstallExtension(
      test_dir.AppendASCII("empty_browser_action_beta.crx"), 1);
  const Extension* extension_c = InstallExtension(
      test_dir.AppendASCII("empty_browser_action_gamma.crx"), 1);
  ASSERT_TRUE(extension_a);
  ASSERT_TRUE(extension_b);
  ASSERT_TRUE(extension_c);

  // Test that there are 3 browser actions in the toolbar.
  ASSERT_EQ(size_before + 3, service->extensions()->size());
  ASSERT_EQ(3, GetBrowserActionsBar().NumberOfBrowserActions());

  // Now enable 2 of the extensions in incognito mode, and test that the browser
  // actions show up.
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());
  prefs->SetIsIncognitoEnabled(extension_a->id(), true);
  prefs->SetIsIncognitoEnabled(extension_c->id(), true);

  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser =
      new Browser(Browser::CreateParams(incognito_profile,
                                        browser()->host_desktop_type()));
  BrowserActionTestUtil incognito_bar(incognito_browser);

  // Navigate just to have a tab in this window, otherwise wonky things happen.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  ASSERT_EQ(2, incognito_bar.NumberOfBrowserActions());

  // Ensure that the browser actions are in the right order (ABC).
  EXPECT_EQ(kTooltipA, GetBrowserActionsBar().GetTooltip(0));
  EXPECT_EQ(kTooltipB, GetBrowserActionsBar().GetTooltip(1));
  EXPECT_EQ(kTooltipC, GetBrowserActionsBar().GetTooltip(2));

  EXPECT_EQ(kTooltipA, incognito_bar.GetTooltip(0));
  EXPECT_EQ(kTooltipC, incognito_bar.GetTooltip(1));

  // Now rearrange them and ensure that they are rearranged correctly in both
  // regular and incognito mode.

  // ABC -> CAB
  ExtensionToolbarModel* toolbar_model = ExtensionToolbarModel::Get(
      browser()->profile());
  toolbar_model->MoveBrowserAction(extension_c, 0);

  EXPECT_EQ(kTooltipC, GetBrowserActionsBar().GetTooltip(0));
  EXPECT_EQ(kTooltipA, GetBrowserActionsBar().GetTooltip(1));
  EXPECT_EQ(kTooltipB, GetBrowserActionsBar().GetTooltip(2));

  EXPECT_EQ(kTooltipC, incognito_bar.GetTooltip(0));
  EXPECT_EQ(kTooltipA, incognito_bar.GetTooltip(1));

  // CAB -> CBA
  toolbar_model->MoveBrowserAction(extension_b, 1);

  EXPECT_EQ(kTooltipC, GetBrowserActionsBar().GetTooltip(0));
  EXPECT_EQ(kTooltipB, GetBrowserActionsBar().GetTooltip(1));
  EXPECT_EQ(kTooltipA, GetBrowserActionsBar().GetTooltip(2));

  EXPECT_EQ(kTooltipC, incognito_bar.GetTooltip(0));
  EXPECT_EQ(kTooltipA, incognito_bar.GetTooltip(1));
}

// Tests that events are dispatched to the correct profile for split mode
// extensions.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, IncognitoSplit) {
  ResultCatcher catcher;
  const Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("browser_action/split_mode"),
      kFlagEnableIncognito);
  ASSERT_TRUE(extension) << message_;

  // Open an incognito window.
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser =
      new Browser(Browser::CreateParams(incognito_profile,
                                        browser()->host_desktop_type()));
  // Navigate just to have a tab in this window, otherwise wonky things happen.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  ASSERT_EQ(1,
            BrowserActionTestUtil(incognito_browser).NumberOfBrowserActions());

  // A click in the regular profile should open a tab in the regular profile.
  ExtensionToolbarModel* toolbar_model = ExtensionToolbarModel::Get(
      browser()->profile());
  toolbar_model->ExecuteBrowserAction(extension, browser(), NULL, true);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // A click in the incognito profile should open a tab in the
  // incognito profile.
  toolbar_model->ExecuteBrowserAction(extension, incognito_browser, NULL, true);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Disabled because of failures (crashes) on ASAN bot.
// See http://crbug.com/98861.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, DISABLED_CloseBackgroundPage) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browser_action/close_background")));
  const Extension* extension = GetSingleLoadedExtension();

  // There is a background page and a browser action with no badge text.
  extensions::ProcessManager* manager =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  ASSERT_TRUE(manager->GetBackgroundHostForExtension(extension->id()));
  ExtensionAction* action = GetBrowserAction(*extension);
  ASSERT_EQ("", action->GetBadgeText(ExtensionAction::kDefaultTabId));

  content::WindowedNotificationObserver host_destroyed_observer(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());

  // Click the browser action.
  ExtensionToolbarModel* toolbar_model = ExtensionToolbarModel::Get(
      browser()->profile());
  toolbar_model->ExecuteBrowserAction(extension, browser(), NULL, true);

  // It can take a moment for the background page to actually get destroyed
  // so we wait for the notification before checking that it's really gone
  // and the badge text has been set.
  host_destroyed_observer.Wait();
  ASSERT_FALSE(manager->GetBackgroundHostForExtension(extension->id()));
  ASSERT_EQ("X", action->GetBadgeText(ExtensionAction::kDefaultTabId));
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, BadgeBackgroundColor) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("browser_action/color")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Test that CSS values (#FF0000) set color correctly.
  ExtensionAction* action = GetBrowserAction(*extension);
  ASSERT_EQ(SkColorSetARGB(255, 255, 0, 0),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that CSS values (#0F0) set color correctly.
  ASSERT_EQ(SkColorSetARGB(255, 0, 255, 0),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that array values set color correctly.
  ASSERT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, Getters) {
  ASSERT_TRUE(RunExtensionTest("browser_action/getters")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Test the getters for defaults.
  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test the getters for a specific tab.
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());
}

// Verify triggering browser action.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, TestTriggerBrowserAction) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("trigger_actions/browser_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(
     browser(),
     test_server()->GetURL("files/simple.html"));

  ExtensionAction* browser_action = GetBrowserAction(*extension);
  EXPECT_TRUE(browser_action != NULL);

  // Simulate a click on the browser action icon.
  {
    ResultCatcher catcher;
    GetBrowserActionsBar().Press(0);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab != NULL);

  // Verify that the browser action turned the background color red.
  const std::string script =
      "window.domAutomationController.send(document.body.style."
      "backgroundColor);";
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(tab, script, &result));
  EXPECT_EQ(result, "red");
}

}  // namespace
}  // namespace extensions
