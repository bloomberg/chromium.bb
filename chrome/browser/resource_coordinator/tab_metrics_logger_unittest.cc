// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/resource_coordinator/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using metrics::TabMetricsEvent;
using content::WebContentsTester;

// Sanity checks for functions in TabMetricsLogger.
// See TabActivityWatcherTest for more thorough tab usage UKM tests.
using TabMetricsLoggerTest = ChromeRenderViewHostTestHarness;

// Tests creating a flat TabFeatures structure for logging a tab and its
// TabMetrics state.
TEST_F(TabMetricsLoggerTest, TabFeatures) {
  TabActivitySimulator tab_activity_simulator;
  Browser::CreateParams params(profile(), true);
  std::unique_ptr<Browser> browser =
      CreateBrowserWithTestWindowForParams(&params);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  // Add a foreground tab.
  tab_activity_simulator.AddWebContentsAndNavigate(tab_strip_model,
                                                   GURL("about://blank"));
  tab_strip_model->ActivateTabAt(0, false);

  // Add a background tab to test.
  content::WebContents* bg_contents =
      tab_activity_simulator.AddWebContentsAndNavigate(
          tab_strip_model, GURL("http://example.com/test.html"));
  WebContentsTester::For(bg_contents)->TestSetIsLoading(false);

  {
    TabMetricsLogger::TabMetrics bg_metrics;
    bg_metrics.web_contents = bg_contents;
    bg_metrics.page_transition = ui::PAGE_TRANSITION_FORM_SUBMIT;

    resource_coordinator::TabFeatures bg_features =
        TabMetricsLogger::GetTabFeatures(browser.get(), bg_metrics);
    EXPECT_EQ(TabMetricsEvent::CONTENT_TYPE_TEXT_HTML,
              bg_features.content_type);
    EXPECT_EQ(bg_features.has_before_unload_handler, false);
    EXPECT_EQ(bg_features.has_form_entry, false);
    EXPECT_EQ(bg_features.is_extension_protected, false);
    EXPECT_EQ(bg_features.is_pinned, false);
    EXPECT_EQ(bg_features.key_event_count, 0);
    EXPECT_EQ(bg_features.mouse_event_count, 0);
    EXPECT_EQ(bg_features.navigation_entry_count, 1);
    ASSERT_TRUE(bg_features.page_transition_core_type.has_value());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        ui::PAGE_TRANSITION_FORM_SUBMIT,
        bg_features.page_transition_core_type.value()));
    EXPECT_EQ(bg_features.page_transition_from_address_bar, false);
    EXPECT_EQ(bg_features.page_transition_is_redirect, false);
    ASSERT_TRUE(bg_features.site_engagement_score.has_value());
    EXPECT_EQ(bg_features.site_engagement_score.value(), 0);
    EXPECT_EQ(bg_features.touch_event_count, 0);
    EXPECT_EQ(bg_features.was_recently_audible, false);
  }

  // Update tab features.
  ui::PageTransition page_transition = static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  tab_activity_simulator.Navigate(bg_contents, GURL("https://www.chromium.org"),
                                  page_transition);
  tab_strip_model->SetTabPinned(1, true);
  // Simulate an extension protecting a tab.
  g_browser_process->GetTabManager()->SetTabAutoDiscardableState(bg_contents,
                                                                 false);
  SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
      GURL("https://www.chromium.org"), 91);

  {
    TabMetricsLogger::TabMetrics bg_metrics;
    bg_metrics.web_contents = bg_contents;
    bg_metrics.page_transition = page_transition;
    bg_metrics.page_metrics.key_event_count = 3;
    bg_metrics.page_metrics.mouse_event_count = 42;
    bg_metrics.page_metrics.touch_event_count = 10;

    resource_coordinator::TabFeatures bg_features =
        TabMetricsLogger::GetTabFeatures(browser.get(), bg_metrics);
    EXPECT_EQ(TabMetricsEvent::CONTENT_TYPE_TEXT_HTML,
              bg_features.content_type);
    EXPECT_EQ(bg_features.has_before_unload_handler, false);
    EXPECT_EQ(bg_features.has_form_entry, false);
    EXPECT_EQ(bg_features.is_extension_protected, true);
    EXPECT_EQ(bg_features.is_pinned, true);
    EXPECT_EQ(bg_features.key_event_count, 3);
    EXPECT_EQ(bg_features.mouse_event_count, 42);
    EXPECT_EQ(bg_features.navigation_entry_count, 2);
    ASSERT_TRUE(bg_features.page_transition_core_type.has_value());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        ui::PAGE_TRANSITION_LINK,
        bg_features.page_transition_core_type.value()));
    EXPECT_EQ(bg_features.page_transition_from_address_bar, true);
    EXPECT_EQ(bg_features.page_transition_is_redirect, false);
    ASSERT_TRUE(bg_features.site_engagement_score.has_value());
    // Site engagement score should round down to the nearest 10.
    EXPECT_EQ(bg_features.site_engagement_score.value(), 90);
    EXPECT_EQ(bg_features.touch_event_count, 10);
    EXPECT_EQ(bg_features.was_recently_audible, false);
  }

  tab_strip_model->CloseAllTabs();
}

// Tests that protocol schemes are mapped to the correct enumerators.
TEST_F(TabMetricsLoggerTest, Schemes) {
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_BITCOIN,
            TabMetricsLogger::GetSchemeValueFromString("bitcoin"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_GEO,
            TabMetricsLogger::GetSchemeValueFromString("geo"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IM,
            TabMetricsLogger::GetSchemeValueFromString("im"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IRC,
            TabMetricsLogger::GetSchemeValueFromString("irc"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IRCS,
            TabMetricsLogger::GetSchemeValueFromString("ircs"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MAGNET,
            TabMetricsLogger::GetSchemeValueFromString("magnet"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MAILTO,
            TabMetricsLogger::GetSchemeValueFromString("mailto"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MMS,
            TabMetricsLogger::GetSchemeValueFromString("mms"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_NEWS,
            TabMetricsLogger::GetSchemeValueFromString("news"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_NNTP,
            TabMetricsLogger::GetSchemeValueFromString("nntp"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OPENPGP4FPR,
            TabMetricsLogger::GetSchemeValueFromString("openpgp4fpr"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SIP,
            TabMetricsLogger::GetSchemeValueFromString("sip"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SMS,
            TabMetricsLogger::GetSchemeValueFromString("sms"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SMSTO,
            TabMetricsLogger::GetSchemeValueFromString("smsto"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SSH,
            TabMetricsLogger::GetSchemeValueFromString("ssh"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_TEL,
            TabMetricsLogger::GetSchemeValueFromString("tel"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_URN,
            TabMetricsLogger::GetSchemeValueFromString("urn"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_WEBCAL,
            TabMetricsLogger::GetSchemeValueFromString("webcal"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_WTAI,
            TabMetricsLogger::GetSchemeValueFromString("wtai"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_XMPP,
            TabMetricsLogger::GetSchemeValueFromString("xmpp"));

  static_assert(
      TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_LAST ==
              TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_XMPP &&
          TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_LAST == 20,
      "This test and the scheme list in TabMetricsLoggerImpl must be updated "
      "when new protocol handlers are added.");
}

// Tests non-whitelisted protocol schemes.
TEST_F(TabMetricsLoggerTest, NonWhitelistedSchemes) {
  // Native (non-web-based) handler.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("foo"));

  // Custom ("web+") protocol handlers.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("web+foo"));
  // "mailto" after the web+ prefix doesn't trigger any special handling.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("web+mailto"));

  // Nonsense protocol handlers.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString(""));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("mailto-xyz"));
}
