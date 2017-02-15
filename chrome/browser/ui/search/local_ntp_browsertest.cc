// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

class LocalNTPTest : public InProcessBrowserTest,
                     public InstantTestBase {
 public:
  LocalNTPTest() {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url =
        https_test_server().GetURL("/instant_extended.html?strk=1&");
    GURL ntp_url =
        https_test_server().GetURL("/local_ntp_browsertest.html?strk=1&");
    InstantTestBase::Init(instant_url, ntp_url, false);
  }
};

// This runs a bunch of pure JS-side tests, i.e. those that don't require any
// interaction from the native side.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, SimpleJavascriptTests) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), ntp_url(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  bool success = false;
  ASSERT_TRUE(GetBoolFromJS(active_tab, "!!runSimpleTests()", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, FakeboxRedirectsToOmnibox) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), ntp_url(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // This is required to make the mouse events we send below arrive at the right
  // place. It *should* be the default for all interactive_ui_tests anyway, but
  // on Mac it isn't; see crbug.com/641969.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  bool result = false;
  ASSERT_TRUE(GetBoolFromJS(active_tab, "!!setupAdvancedTest()", &result));
  ASSERT_TRUE(result);

  // Get the position of the fakebox on the page.
  double fakebox_x = 0;
  ASSERT_TRUE(GetDoubleFromJS(active_tab, "getFakeboxPositionX()", &fakebox_x));
  double fakebox_y = 0;
  ASSERT_TRUE(GetDoubleFromJS(active_tab, "getFakeboxPositionY()", &fakebox_y));

  // Move the mouse over the fakebox.
  gfx::Vector2d fakebox_pos(static_cast<int>(std::ceil(fakebox_x)),
                            static_cast<int>(std::ceil(fakebox_y)));
  gfx::Point origin = active_tab->GetContainerBounds().origin();
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(origin + fakebox_pos +
                                               gfx::Vector2d(1, 1)));

  // Click on the fakebox, and wait for the omnibox to receive invisible focus.
  // Note that simply waiting for the first OMNIBOX_FOCUS_CHANGED notification
  // is not sufficient: If the omnibox had focus before, it will first lose the
  // focus before gaining invisible focus.
  ASSERT_NE(OMNIBOX_FOCUS_INVISIBLE, omnibox()->model()->focus_state());
  content::WindowedNotificationObserver focus_observer(
      chrome::NOTIFICATION_OMNIBOX_FOCUS_CHANGED,
      base::Bind(
          [](const OmniboxEditModel* omnibox_model) {
            return omnibox_model->focus_state() == OMNIBOX_FOCUS_INVISIBLE;
          },
          omnibox()->model()));

  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));

  focus_observer.Wait();
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE, omnibox()->model()->focus_state());

  // The fakebox should now also have focus.
  ASSERT_TRUE(GetBoolFromJS(active_tab, "!!fakeboxIsFocused()", &result));
  EXPECT_TRUE(result);

  // Type "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::KeyboardCode::VKEY_A,
      /*control=*/false, /*shift=*/false, /*alt=*/false, /*command=*/false));

  // The omnibox should have received visible focus.
  EXPECT_EQ(OMNIBOX_FOCUS_VISIBLE, omnibox()->model()->focus_state());
  // ...and the typed text should have arrived there.
  EXPECT_EQ("a", GetOmniboxText());

  // On the JS side, the fakebox should have been hidden.
  ASSERT_TRUE(GetBoolFromJS(active_tab, "!!fakeboxIsVisible()", &result));
  EXPECT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       NTPRespectsBrowserLanguageSetting) {
  // Make sure the default language is not French.
  std::string default_locale = g_browser_process->GetApplicationLocale();
  EXPECT_NE("fr", default_locale);

  // Switch browser language to French.
  std::string loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("fr");

  // The platform cannot load the French locale (GetApplicationLocale() is
  // platform specific, and has been observed to fail on a small number of
  // platforms). Abort the test.
  if (loaded_locale != "fr")
    return;

  g_browser_process->SetApplicationLocale(loaded_locale);
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, loaded_locale);

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Verify that the NTP is in French.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());
}
