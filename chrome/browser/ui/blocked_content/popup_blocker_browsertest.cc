// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using content::WebContents;
using content::NativeWebKeyboardEvent;

namespace {

// Counts the number of RenderViewHosts created.
class CountRenderViewHosts : public content::NotificationObserver {
 public:
  CountRenderViewHosts()
      : count_(0) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                   content::NotificationService::AllSources());
  }
  ~CountRenderViewHosts() override {}

  int GetRenderViewHostCreatedCount() const { return count_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    count_++;
  }

  content::NotificationRegistrar registrar_;

  int count_;

  DISALLOW_COPY_AND_ASSIGN(CountRenderViewHosts);
};

class CloseObserver : public content::WebContentsObserver {
 public:
  explicit CloseObserver(WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void Wait() {
    close_loop_.Run();
  }

  void WebContentsDestroyed() override { close_loop_.Quit(); }

 private:
  base::RunLoop close_loop_;

  DISALLOW_COPY_AND_ASSIGN(CloseObserver);
};

class BrowserActivationObserver : public chrome::BrowserListObserver {
 public:
  BrowserActivationObserver()
      : browser_(chrome::FindLastActive()), observed_(false) {
    BrowserList::AddObserver(this);
  }
  ~BrowserActivationObserver() override { BrowserList::RemoveObserver(this); }

  void WaitForActivation() {
    if (observed_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  // chrome::BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override {
    if (browser == browser_)
      return;
    if (browser->host_desktop_type() != browser_->host_desktop_type())
      return;
    observed_ = true;
    if (message_loop_runner_.get() && message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  Browser* browser_;
  bool observed_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivationObserver);
};

class PopupBlockerBrowserTest : public InProcessBrowserTest {
 public:
  PopupBlockerBrowserTest() {}
  ~PopupBlockerBrowserTest() override {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  int GetBlockedContentsCount() {
    // Do a round trip to the renderer first to flush any in-flight IPCs to
    // create a to-be-blocked window.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    CHECK(content::ExecuteScript(tab, std::string()));
    PopupBlockerTabHelper* popup_blocker_helper =
        PopupBlockerTabHelper::FromWebContents(tab);
    return popup_blocker_helper->GetBlockedPopupsCount();
  }

  enum WhatToExpect {
    ExpectPopup,
    ExpectTab
  };

  enum ShouldCheckTitle {
    CheckTitle,
    DontCheckTitle
  };

  void NavigateAndCheckPopupShown(const GURL& url,
                                  WhatToExpect what_to_expect) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();

    if (what_to_expect == ExpectPopup) {
      ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    } else {
      ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
      ASSERT_EQ(2, browser()->tab_strip_model()->count());

      // Check that we always create foreground tabs.
      ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
    }

    ASSERT_EQ(0, GetBlockedContentsCount());
  }

  // Navigates to the test indicated by |test_name| using |browser| which is
  // expected to try to open a popup. Verifies that the popup was blocked and
  // then opens the blocked popup. Once the popup stopped loading, verifies
  // that the title of the page is "PASS" if |check_title| is set.
  //
  // If |what_to_expect| is ExpectPopup, the popup is expected to open a new
  // window, or a background tab if it is false.
  //
  // Returns the WebContents of the launched popup.
  WebContents* RunCheckTest(Browser* browser,
                            const std::string& test_name,
                            WhatToExpect what_to_expect,
                            ShouldCheckTitle check_title) {
    GURL url(embedded_test_server()->GetURL(test_name));

    CountRenderViewHosts counter;

    ui_test_utils::NavigateToURL(browser, url);

    // Since the popup blocker blocked the window.open, there should be only one
    // tab.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    // And no new RVH created.
    EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    ui_test_utils::BrowserAddedObserver browser_observer;

    // Launch the blocked popup.
    PopupBlockerTabHelper* popup_blocker_helper =
        PopupBlockerTabHelper::FromWebContents(web_contents);
    if (!popup_blocker_helper->GetBlockedPopupsCount()) {
      content::WindowedNotificationObserver observer(
          chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
          content::NotificationService::AllSources());
      observer.Wait();
    }
    EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
    std::map<int32_t, GURL> blocked_requests =
        popup_blocker_helper->GetBlockedPopupRequests();
    std::map<int32_t, GURL>::const_iterator iter = blocked_requests.begin();
    popup_blocker_helper->ShowBlockedPopup(iter->first);

    observer.Wait();
    Browser* new_browser;
    if (what_to_expect == ExpectPopup) {
      new_browser = browser_observer.WaitForSingleNewBrowser();
      web_contents = new_browser->tab_strip_model()->GetActiveWebContents();
    } else {
      new_browser = browser;
      EXPECT_EQ(2, browser->tab_strip_model()->count());
      // Check that we always create foreground tabs.
      EXPECT_EQ(1, browser->tab_strip_model()->active_index());
      web_contents = browser->tab_strip_model()->GetWebContentsAt(1);
    }

    if (check_title == CheckTitle) {
      // Check that the check passed.
      base::string16 expected_title(base::ASCIIToUTF16("PASS"));
      content::TitleWatcher title_watcher(web_contents, expected_title);
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }

    return web_contents;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupBlockerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       BlockWebContentsCreation) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      browser(),
      "/popup_blocker/popup-blocked-to-post-blank.html",
      ExpectTab,
      DontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       BlockWebContentsCreationIncognito) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      CreateIncognitoBrowser(),
      "/popup_blocker/popup-blocked-to-post-blank.html",
      ExpectTab,
      DontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchor) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      browser(),
      "/popup_blocker/popup-fake-click-on-anchor.html",
      ExpectTab,
      DontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchorNoTarget) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      browser(),
      "/popup_blocker/popup-fake-click-on-anchor2.html",
      ExpectTab,
      DontCheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MultiplePopups) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-many.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(2, GetBlockedContentsCount());
}

// Verify that popups are launched on browser back button.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSetting) {
  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-blocked-to-post-blank.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, ExpectTab);
}

// Verify that content settings are applied based on the top-level frame URL.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSettingIFrame) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-frames.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);

  // Popup from the iframe should be allowed since the top-level URL is
  // whitelisted.
  NavigateAndCheckPopupShown(url, ExpectTab);

  // Whitelist iframe URL instead.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("www.a.com");
  GURL frame_url(embedded_test_server()
                     ->GetURL("/popup_blocker/popup-frames-iframe.html")
                     .ReplaceComponents(replace_host));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSetting(ContentSettingsPattern::FromURL(frame_url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);

  // Popup should be blocked.
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(1, GetBlockedContentsCount());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupsLaunchWhenTabIsClosed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  NavigateAndCheckPopupShown(embedded_test_server()->GetURL("/popup_blocker/"),
                             ExpectPopup);
}

// Verify that when you unblock popup, the popup shows in history and omnibox.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       UnblockedPopupShowsInHistoryAndOmnibox) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-blocked-to-post-blank.html"));
  NavigateAndCheckPopupShown(url, ExpectTab);

  std::string search_string =
      "data:text/html,<title>Popup Success!</title>you should not see this "
      "message if popup blocker is enabled";

  ui_test_utils::HistoryEnumerator history(browser()->profile());
  std::vector<GURL>& history_urls = history.urls();
  ASSERT_EQ(2u, history_urls.size());
  ASSERT_EQ(GURL(search_string), history_urls[0]);
  ASSERT_EQ(url, history_urls[1]);

  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(service);
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, search_string);
  OmniboxEditModel* model = location_bar->GetOmniboxView()->model();
  EXPECT_EQ(GURL(search_string), model->CurrentMatch(NULL).destination_url);
  EXPECT_EQ(base::ASCIIToUTF16(search_string),
            model->CurrentMatch(NULL).contents);
}

// This test fails on linux AURA with this change
// https://codereview.chromium.org/23903056
// BUG=https://code.google.com/p/chromium/issues/detail?id=295299
// TODO(ananta). Debug and fix this test.
#if defined(USE_AURA) && defined(OS_LINUX)
#define MAYBE_WindowFeatures DISABLED_WindowFeatures
#else
#define MAYBE_WindowFeatures WindowFeatures
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MAYBE_WindowFeatures) {
  WebContents* popup =
      RunCheckTest(browser(),
                   "/popup_blocker/popup-window-open.html",
                   ExpectPopup,
                   DontCheckTitle);

  // Check that the new popup has (roughly) the requested size.
  gfx::Size window_size = popup->GetContainerBounds().size();
  EXPECT_TRUE(349 <= window_size.width() && window_size.width() <= 351);
  EXPECT_TRUE(249 <= window_size.height() && window_size.height() <= 251);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, CorrectReferrer) {
  RunCheckTest(browser(),
               "/popup_blocker/popup-referrer.html",
               ExpectTab,
               CheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WindowFeaturesBarProps) {
  RunCheckTest(browser(),
               "/popup_blocker/popup-windowfeatures.html",
               ExpectPopup,
               CheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, SessionStorage) {
  RunCheckTest(browser(),
               "/popup_blocker/popup-sessionstorage.html",
               ExpectTab,
               CheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, Opener) {
  RunCheckTest(browser(),
               "/popup_blocker/popup-opener.html",
               ExpectTab,
               CheckTitle);
}

// Tests that the popup can still close itself after navigating. This tests that
// the openedByDOM bit is preserved across blocked popups.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ClosableAfterNavigation) {
  // Open a popup.
  WebContents* popup =
      RunCheckTest(browser(),
                   "/popup_blocker/popup-opener.html",
                   ExpectTab,
                   CheckTitle);

  // Navigate it elsewhere.
  content::TestNavigationObserver nav_observer(popup);
  popup->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("location.href = '/empty.html'"));
  nav_observer.Wait();

  // Have it close itself.
  CloseObserver close_observer(popup);
  popup->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("window.close()"));
  close_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, OpenerSuppressed) {
  RunCheckTest(browser(),
               "/popup_blocker/popup-openersuppressed.html",
               ExpectTab,
               CheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ShiftClick) {
  RunCheckTest(
      browser(),
      "/popup_blocker/popup-fake-click-on-anchor3.html",
      ExpectPopup,
      CheckTitle);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WebUI) {
  WebContents* popup =
      RunCheckTest(browser(),
                   "/popup_blocker/popup-webui.html",
                   ExpectTab,
                   DontCheckTitle);

  // Check that the new popup displays about:blank.
  EXPECT_EQ(GURL(url::kAboutBlankURL), popup->GetURL());
}

// Verify that the renderer can't DOS the browser by creating arbitrarily many
// popups.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DenialOfService) {
  GURL url(embedded_test_server()->GetURL("/popup_blocker/popup-dos.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(25, GetBlockedContentsCount());
}

// Verify that an onunload popup does not show up for about:blank.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, Regress427477) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-on-unload.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  tab->GetController().GoBack();
  content::WaitForLoadStop(tab);

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // The popup from the unload event handler should not show up for about:blank.
  ASSERT_EQ(0, GetBlockedContentsCount());
}

// Verify that app modal prompts can't be used to create pop unders.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ModalPopUnder) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(
      embedded_test_server()->GetURL("/popup_blocker/popup-window-open.html"));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url, ExpectPopup);

  Browser* popup_browser = chrome::FindLastActive();
  ASSERT_NE(popup_browser, browser());

  // Showing an alert will raise the tab over the popup.
  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16("alert()"));
  app_modal::AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();

  // Verify that after the dialog was closed, the popup is in front again.
  ASSERT_TRUE(dialog->IsJavaScriptModalDialog());
  app_modal::JavaScriptAppModalDialog* js_dialog =
      static_cast<app_modal::JavaScriptAppModalDialog*>(dialog);

  BrowserActivationObserver activation_observer;
  js_dialog->native_dialog()->AcceptAppModalDialog();

  if (popup_browser != chrome::FindLastActive())
    activation_observer.WaitForActivation();
  ASSERT_EQ(popup_browser, chrome::FindLastActive());
}

void BuildSimpleWebKeyEvent(blink::WebInputEvent::Type type,
                            ui::KeyboardCode key_code,
                            int native_key_code,
                            int modifiers,
                            NativeWebKeyboardEvent* event) {
  event->nativeKeyCode = native_key_code;
  event->windowsKeyCode = key_code;
  event->setKeyIdentifierFromWindowsKeyCode();
  event->type = type;
  event->modifiers = modifiers;
  event->isSystemKey = false;
  event->timeStampSeconds = base::Time::Now().ToDoubleT();
  event->skip_in_browser = true;

  if (type == blink::WebInputEvent::Char ||
      type == blink::WebInputEvent::RawKeyDown) {
    event->text[0] = key_code;
    event->unmodifiedText[0] = key_code;
  }
}

void InjectRawKeyEvent(WebContents* web_contents,
                       blink::WebInputEvent::Type type,
                       ui::KeyboardCode key_code,
                       int native_key_code,
                       int modifiers) {
  NativeWebKeyboardEvent event;
  BuildSimpleWebKeyEvent(type, key_code, native_key_code, modifiers, &event);
  web_contents->GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
}

// Tests that Ctrl+Enter/Cmd+Enter keys on a link open the backgournd tab.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, CtrlEnterKey) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WindowedNotificationObserver wait_for_new_tab(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

#if defined(OS_MACOSX)
  int modifiers = blink::WebInputEvent::MetaKey;
  InjectRawKeyEvent(
      tab, blink::WebInputEvent::RawKeyDown, ui::VKEY_COMMAND,
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::OS_LEFT),
      modifiers);
#else
  int modifiers = blink::WebInputEvent::ControlKey;
  InjectRawKeyEvent(
      tab, blink::WebInputEvent::RawKeyDown, ui::VKEY_CONTROL,
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::CONTROL_LEFT),
      modifiers);
#endif

  InjectRawKeyEvent(tab, blink::WebInputEvent::RawKeyDown, ui::VKEY_RETURN,
                    ui::KeycodeConverter::InvalidNativeKeycode(), modifiers);

  InjectRawKeyEvent(tab, blink::WebInputEvent::Char, ui::VKEY_RETURN,
                    ui::KeycodeConverter::InvalidNativeKeycode(), modifiers);

  InjectRawKeyEvent(tab, blink::WebInputEvent::KeyUp, ui::VKEY_RETURN,
                    ui::KeycodeConverter::InvalidNativeKeycode(), modifiers);

#if defined(OS_MACOSX)
  InjectRawKeyEvent(
      tab, blink::WebInputEvent::KeyUp, ui::VKEY_COMMAND,
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::OS_LEFT),
      modifiers);
#else
  InjectRawKeyEvent(
      tab, blink::WebInputEvent::KeyUp, ui::VKEY_CONTROL,
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::CONTROL_LEFT),
      modifiers);
#endif
  wait_for_new_tab.Wait();

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  // Check that we create the background tab.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Tests that the tapping gesture with cntl/cmd key on a link open the
// backgournd tab.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, TapGestureWithCtrlKey) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL url(embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor2.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WindowedNotificationObserver wait_for_new_tab(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

#if defined(OS_MACOSX)
  unsigned modifiers = blink::WebInputEvent::MetaKey;
#else
  unsigned modifiers = blink::WebInputEvent::ControlKey;
#endif
  content::SimulateTapWithModifiersAt(tab, modifiers, gfx::Point(350, 250));

  wait_for_new_tab.Wait();

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  // Check that we create the background tab.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

}  // namespace
