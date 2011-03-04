// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/scoped_temp_dir.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/net/url_request_slow_http_job.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/autocomplete_edit_proxy.h"
#include "chrome/test/automation/automation_proxy_uitest.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/proxy_launcher.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/message_box_flags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect.h"

using ui_test_utils::TimedMessageLoopRunner;
using testing::CreateFunctor;
using testing::StrEq;
using testing::_;


// Replace the default automation proxy with our mock client.
class ExternalTabUITestMockLauncher : public ProxyLauncher {
 public:
  explicit ExternalTabUITestMockLauncher(ExternalTabUITestMockClient **mock)
      : mock_(mock) {
    channel_id_ = AutomationProxy::GenerateChannelID();
  }

  AutomationProxy* CreateAutomationProxy(int execution_timeout) {
    *mock_ = new ExternalTabUITestMockClient(execution_timeout);
    (*mock_)->InitializeChannel(channel_id_, false);
    return *mock_;
  }

  void InitializeConnection(const LaunchState& state,
                            bool wait_for_initial_loads) {
    LaunchBrowserAndServer(state, wait_for_initial_loads);
  }

  void TerminateConnection() {
    CloseBrowserAndServer();
  }

  std::string PrefixedChannelID() const {
    return channel_id_;
  }

 private:
  ExternalTabUITestMockClient **mock_;
  std::string channel_id_;      // Channel id of automation proxy.
};

class AutomationProxyTest : public UITest {
 protected:
  AutomationProxyTest() {
    dom_automation_enabled_ = true;
    launch_arguments_.AppendSwitchASCII(switches::kLang, "en-US");
  }
};

TEST_F(AutomationProxyTest, GetBrowserWindowCount) {
  int window_count = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
#ifdef NDEBUG
  ASSERT_FALSE(automation()->GetBrowserWindowCount(NULL));
#endif
}

TEST_F(AutomationProxyTest, GetBrowserWindow) {
  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(window.get());
  }

  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(-1));
    ASSERT_FALSE(window.get());
  }

  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(1));
    ASSERT_FALSE(window.get());
  }
};

#if defined(OS_MACOSX)
// Missing automation provider support: http://crbug.com/45892
#define MAYBE_WindowGetViewBounds FAILS_WindowGetViewBounds
#else
#define MAYBE_WindowGetViewBounds WindowGetViewBounds
#endif
TEST_F(AutomationProxyVisibleTest, MAYBE_WindowGetViewBounds) {
  {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    scoped_refptr<WindowProxy> window(browser->GetWindow());
    ASSERT_TRUE(window.get());

    scoped_refptr<TabProxy> tab1(browser->GetTab(0));
    ASSERT_TRUE(tab1.get());
    GURL tab1_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

    // Add another tab so we can simulate dragging.
    ASSERT_TRUE(browser->AppendTab(GURL("about:")));

    scoped_refptr<TabProxy> tab2(browser->GetTab(1));
    ASSERT_TRUE(tab2.get());
    GURL tab2_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_url));

    EXPECT_NE(tab1_url.spec(), tab2_url.spec());

    gfx::Rect bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds, false));
    EXPECT_GT(bounds.width(), 0);
    EXPECT_GT(bounds.height(), 0);

    gfx::Rect bounds2;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_LAST, &bounds2, false));
    EXPECT_GT(bounds2.x(), 0);
    EXPECT_GT(bounds2.width(), 0);
    EXPECT_GT(bounds2.height(), 0);

    // The tab logic is mirrored in RTL locales, so what is to the right in
    // LTR locales is now on the left with RTL ones.
    string16 browser_locale;

    EXPECT_TRUE(automation()->GetBrowserLocale(&browser_locale));

    const std::string& locale_utf8 = UTF16ToUTF8(browser_locale);
    if (base::i18n::GetTextDirectionForLocale(locale_utf8.c_str()) ==
        base::i18n::RIGHT_TO_LEFT) {
      EXPECT_LT(bounds2.x(), bounds.x());
    } else {
      EXPECT_GT(bounds2.x(), bounds.x());
    }
    EXPECT_EQ(bounds2.y(), bounds.y());

    gfx::Rect urlbar_bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                      false));
    EXPECT_GT(urlbar_bounds.x(), 0);
    EXPECT_GT(urlbar_bounds.y(), 0);
    EXPECT_GT(urlbar_bounds.width(), 0);
    EXPECT_GT(urlbar_bounds.height(), 0);

    /*

    TODO(beng): uncomment this section or move to interactive_ui_tests post
    haste!

    // Now that we know where the tabs are, let's try dragging one.
    POINT start;
    POINT end;
    start.x = bounds.x() + bounds.width() / 2;
    start.y = bounds.y() + bounds.height() / 2;
    end.x = start.x + 2 * bounds.width() / 3;
    end.y = start.y;
    ASSERT_TRUE(browser->SimulateDrag(start, end, ui::EF_LEFT_BUTTON_DOWN));

    // Check to see that the drag event successfully swapped the two tabs.
    tab1 = browser->GetTab(0);
    ASSERT_TRUE(tab1.get());
    GURL tab1_new_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

    tab2 = browser->GetTab(1);
    ASSERT_TRUE(tab2.get());
    GURL tab2_new_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

    EXPECT_EQ(tab1_url.spec(), tab2_new_url.spec());
    EXPECT_EQ(tab2_url.spec(), tab1_new_url.spec());

    */
  }
}

TEST_F(AutomationProxyTest, GetTabCount) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = 0;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);
}

TEST_F(AutomationProxyTest, GetActiveTabIndex) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);
}

TEST_F(AutomationProxyVisibleTest, AppendTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int original_tab_count;
  ASSERT_TRUE(window->GetTabCount(&original_tab_count));
  ASSERT_EQ(1, original_tab_count);  // By default there are 2 tabs opened.

  int original_active_tab_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&original_active_tab_index));
  ASSERT_EQ(0, original_active_tab_index);  // By default 0-th tab is active

  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));
  int tab_count;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(original_tab_count + 1, tab_count);

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(tab_count - 1, active_tab_index);
  ASSERT_NE(original_active_tab_index, active_tab_index);

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(filename)));

  int appended_tab_index;
  // Append tab will also be active tab
  ASSERT_TRUE(window->GetActiveTabIndex(&appended_tab_index));

  scoped_refptr<TabProxy> tab(window->GetTab(appended_tab_index));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, ActivateTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));

  ASSERT_TRUE(window->ActivateTab(1));
  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);

  ASSERT_TRUE(window->ActivateTab(0));
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);
}

TEST_F(AutomationProxyTest, GetTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  {
    scoped_refptr<TabProxy> tab(window->GetTab(0));
    ASSERT_TRUE(tab.get());
    std::wstring title;
    ASSERT_TRUE(tab->GetTabTitle(&title));
    ASSERT_STREQ(L"about:blank", title.c_str());
  }

  {
    ASSERT_FALSE(window->GetTab(-1));
  }

  {
    scoped_refptr<TabProxy> tab(window->GetTab(1));
    ASSERT_FALSE(tab.get());
  }
};

TEST_F(AutomationProxyTest, NavigateToURL) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"about:blank", title.c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(filename)));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  // TODO(vibhor) : Add a test using testserver.
}

TEST_F(AutomationProxyTest, GoBackForward) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"about:blank", title.c_str());

  ASSERT_FALSE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"about:blank", title.c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  ASSERT_TRUE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"about:blank", title.c_str());

  ASSERT_TRUE(tab->GoForward());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  ASSERT_FALSE(tab->GoForward());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, GetCurrentURL) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  GURL url;
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  ASSERT_STREQ("about:blank", url.spec().c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);
  ASSERT_TRUE(tab->NavigateToURL(newurl));
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  // compare canonical urls...
  ASSERT_STREQ(newurl.spec().c_str(), url.spec().c_str());
}

class AutomationProxyTest2 : public AutomationProxyVisibleTest {
 protected:
  AutomationProxyTest2() {
    document1_= test_data_directory_.AppendASCII("title1.html");

    document2_ = test_data_directory_.AppendASCII("title2.html");
    launch_arguments_ = CommandLine(CommandLine::NO_PROGRAM);
    launch_arguments_.AppendArgPath(document1_);
    launch_arguments_.AppendArgPath(document2_);
  }

  FilePath document1_;
  FilePath document2_;
};

TEST_F(AutomationProxyTest2, GetActiveTabIndex) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);

  ASSERT_TRUE(window->ActivateTab(1));
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);
}

TEST_F(AutomationProxyTest2, GetTabTitle) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"title1.html", title.c_str());

  tab = window->GetTab(1);
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, Cookies) {
  GURL url("http://mojo.jojo.google.com");
  std::string value_result;

  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  // test setting the cookie:
  ASSERT_TRUE(tab->SetCookie(url, "foo=baz"));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("baz", value_result.c_str());

  // test clearing the cookie:
  ASSERT_TRUE(tab->SetCookie(url, "foo="));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo", &value_result));
  ASSERT_TRUE(value_result.empty());

  // now, test that we can get multiple cookies:
  ASSERT_TRUE(tab->SetCookie(url, "foo1=baz1"));
  ASSERT_TRUE(tab->SetCookie(url, "foo2=baz2"));

  ASSERT_TRUE(tab->GetCookies(url, &value_result));
  ASSERT_FALSE(value_result.empty());
  EXPECT_TRUE(value_result.find("foo1=baz1") != std::string::npos);
  EXPECT_TRUE(value_result.find("foo2=baz2") != std::string::npos);

  // test deleting cookie
  ASSERT_TRUE(tab->SetCookie(url, "foo3=deleteme"));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo3", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("deleteme", value_result.c_str());

  ASSERT_TRUE(tab->DeleteCookie(url, "foo3"));
}

TEST_F(AutomationProxyTest, NavigateToURLAsync) {
  AutomationProxy* automation_object = automation();
  scoped_refptr<BrowserProxy> window(automation_object->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);

  ASSERT_TRUE(tab->NavigateToURLAsync(newurl));
  std::string value = WaitUntilCookieNonEmpty(
      tab.get(), newurl, "foo", TestTimeouts::action_max_timeout_ms());
  ASSERT_STREQ("baz", value.c_str());
}

TEST_F(AutomationProxyTest, AcceleratorNewTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = -1;
  ASSERT_TRUE(window->RunCommand(IDC_NEW_TAB));
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  EXPECT_EQ(2, tab_count);

  scoped_refptr<TabProxy> tab(window->GetTab(1));
  ASSERT_TRUE(tab.get());
}

TEST_F(AutomationProxyTest, AcceleratorDownloads) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->RunCommand(IDC_SHOW_DOWNLOADS));

  // We expect the RunCommand above to wait until the title is updated.
  EXPECT_EQ(L"Downloads", GetActiveTabTitle());
}

TEST_F(AutomationProxyTest, AcceleratorExtensions) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->RunCommand(IDC_MANAGE_EXTENSIONS));

  // We expect the RunCommand above to wait until the title is updated.
  EXPECT_EQ(L"Extensions", GetActiveTabTitle());
}

TEST_F(AutomationProxyTest, AcceleratorHistory) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->RunCommand(IDC_SHOW_HISTORY));

  // We expect the RunCommand above to wait until the title is updated.
  EXPECT_EQ(L"History", GetActiveTabTitle());
}

class AutomationProxyTest4 : public UITest {
 protected:
  AutomationProxyTest4() : UITest() {
    dom_automation_enabled_ = true;
  }
};

std::wstring CreateJSString(const std::wstring& value) {
  std::wstring jscript;
  base::SStringPrintf(&jscript,
      L"window.domAutomationController.send(%ls);",
      value.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest4, StringValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring expected(L"string");
  std::wstring jscript = CreateJSString(L"\"" + expected + L"\"");
  std::wstring actual;
  ASSERT_TRUE(tab->ExecuteAndExtractString(L"", jscript, &actual));
  ASSERT_STREQ(expected.c_str(), actual.c_str());
}

std::wstring BooleanToString(bool bool_value) {
  Value* value = Value::CreateBooleanValue(bool_value);
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(*value);
  return UTF8ToWide(json_string);
}

TEST_F(AutomationProxyTest4, BooleanValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool expected = true;
  std::wstring jscript = CreateJSString(BooleanToString(expected));
  bool actual = false;
  ASSERT_TRUE(tab->ExecuteAndExtractBool(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

TEST_F(AutomationProxyTest4, NumberValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  int expected = 1;
  int actual = 0;
  std::wstring expected_string;
  base::SStringPrintf(&expected_string, L"%d", expected);
  std::wstring jscript = CreateJSString(expected_string);
  ASSERT_TRUE(tab->ExecuteAndExtractInt(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

// TODO(vibhor): Add a test for ExecuteAndExtractValue() for JSON Dictionary
// type value

class AutomationProxyTest3 : public UITest {
 protected:
  AutomationProxyTest3() : UITest() {
    document1_ = test_data_directory_;
    document1_ = document1_.AppendASCII("frame_dom_access");
    document1_ = document1_.AppendASCII("frame_dom_access.html");

    dom_automation_enabled_ = true;
    launch_arguments_ = CommandLine(CommandLine::NO_PROGRAM);
    launch_arguments_.AppendArgPath(document1_);
  }

  FilePath document1_;
};

std::wstring CreateJSStringForDOMQuery(const std::wstring& id) {
  std::wstring jscript(L"window.domAutomationController");
  base::StringAppendF(&jscript,
                      L".send(document.getElementById('%ls').nodeName);",
                      id.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest3, FrameDocumentCanBeAccessed) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring actual;
  std::wstring xpath1 = L"";  // top level frame
  std::wstring jscript1 = CreateJSStringForDOMQuery(L"myinput");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath1, jscript1, &actual));
  ASSERT_EQ(L"INPUT", actual);

  std::wstring xpath2 = L"/html/body/iframe";
  std::wstring jscript2 = CreateJSStringForDOMQuery(L"myspan");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath2, jscript2, &actual));
  ASSERT_EQ(L"SPAN", actual);

  std::wstring xpath3 = L"/html/body/iframe\n/html/body/iframe";
  std::wstring jscript3 = CreateJSStringForDOMQuery(L"mydiv");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath3, jscript3, &actual));
  ASSERT_EQ(L"DIV", actual);
}

// Flaky, http://crbug.com/70937
TEST_F(AutomationProxyTest, FLAKY_BlockedPopupTest) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("constrained_files");
  filename = filename.AppendASCII("constrained_window.html");

  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));

  ASSERT_TRUE(tab->WaitForBlockedPopupCountToChangeTo(
      2, TestTimeouts::action_max_timeout_ms()));
}

// TODO(port): Remove HWND if possible
#if defined(OS_WIN)

const char simple_data_url[] =
    "data:text/html,<html><head><title>External tab test</title></head>"
    "<body>A simple page for testing a floating/invisible tab<br></div>"
    "</body></html>";

ExternalTabUITestMockClient::ExternalTabUITestMockClient(int execution_timeout)
    : AutomationProxy(execution_timeout, false),
      host_window_style_(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE),
      host_window_(NULL) {
}

ExternalTabUITestMockClient::~ExternalTabUITestMockClient() {
  EXPECT_TRUE(host_window_ == NULL);
}

void ExternalTabUITestMockClient::ReplyStarted(
    const AutomationURLResponse* response,
    int tab_handle, int request_id) {
  AutomationProxy::Send(new AutomationMsg_RequestStarted(tab_handle,
      request_id, *response));
}

void ExternalTabUITestMockClient::ReplyData(
    const std::string* data, int tab_handle, int request_id) {
  AutomationProxy::Send(new AutomationMsg_RequestData(tab_handle,
                                                      request_id, *data));
}

void ExternalTabUITestMockClient::ReplyEOF(int tab_handle, int request_id) {
  ReplyEnd(net::URLRequestStatus(), tab_handle, request_id);
}

void ExternalTabUITestMockClient::ReplyEnd(const net::URLRequestStatus& status,
                                           int tab_handle, int request_id) {
  AutomationProxy::Send(new AutomationMsg_RequestEnd(tab_handle,
                                                     request_id, status));
}

void ExternalTabUITestMockClient::Reply404(int tab_handle, int request_id) {
  const AutomationURLResponse notfound("", "HTTP/1.1 404\r\n\r\n", 0,
                                       base::Time(), "", 0,
                                       net::HostPortPair());
  ReplyStarted(&notfound, tab_handle, request_id);
  ReplyEOF(tab_handle, request_id);
}

void ExternalTabUITestMockClient::ServeHTMLData(int tab_handle,
                                                const GURL& url,
                                                const std::string& data) {
  EXPECT_CALL(*this, OnRequestStart(tab_handle, testing::AllOf(
    testing::Field(&AutomationURLRequest::url, url.spec()),
      testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
    .Times(1)
    .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(this,
        &ExternalTabUITestMockClient::ReplyStarted, &http_200))));

  EXPECT_CALL(*this, OnRequestRead(tab_handle, testing::Gt(0)))
      .Times(2)
      .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(this,
          &ExternalTabUITestMockClient::ReplyData, &data))))
      .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(this,
          &ExternalTabUITestMockClient::ReplyEOF))));
}

void ExternalTabUITestMockClient::IgnoreFavIconNetworkRequest() {
  // Ignore favicon.ico
  EXPECT_CALL(*this, OnRequestStart(_, testing::AllOf(
          testing::Field(&AutomationURLRequest::url,
                         testing::EndsWith("favicon.ico")),
          testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::WithArgs<0, 0>(testing::Invoke(
          CreateFunctor(this, &ExternalTabUITestMockClient::ReplyEnd,
                net::URLRequestStatus(net::URLRequestStatus::FAILED, 0)))));
}

void ExternalTabUITestMockClient::InvalidateHandle(
    const IPC::Message& message) {
  void* iter = NULL;
  int handle;
  ASSERT_TRUE(message.ReadInt(&iter, &handle));

  // Call base class
  AutomationProxy::InvalidateHandle(message);
  HandleClosed(handle);
}

// Most of the time we need external tab with these settings.
const ExternalTabSettings ExternalTabUITestMockClient::default_settings(
    NULL, gfx::Rect(),  // will be replaced by CreateHostWindowAndTab
    WS_CHILD | WS_VISIBLE,
    false,   // is_off_the_record
    true,    // load_requests_via_automation
    true,    // handle_top_level_requests
    GURL(),  // initial_url
    GURL(),  // referrer
    false,   // infobars_enabled
    false);  // route_all_top_level_navigations

// static
const AutomationURLResponse ExternalTabUITestMockClient::http_200(
    "",
    "HTTP/0.9 200\r\n\r\n",
    0,
    base::Time(),
    "",
    0,
    net::HostPortPair());

bool ExternalTabUITestMockClient::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExternalTabUITestMockClient, msg)
    IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate, OnDidNavigate)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
        OnForwardMessageToExternalHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStart, OnRequestStart)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestRead, OnRequestRead)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookieAsync, OnSetCookieAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabLoaded, OnLoad)
    IPC_MESSAGE_HANDLER(AutomationMsg_AttachExternalTab, OnAttachExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationStateChanged,
                        OnNavigationStateChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

scoped_refptr<TabProxy> ExternalTabUITestMockClient::CreateHostWindowAndTab(
    const ExternalTabSettings& settings) {
  EXPECT_THAT(settings.parent, testing::IsNull());

  host_window_ = CreateWindowW(L"Button", NULL, host_window_style_,
      CW_USEDEFAULT,  CW_USEDEFAULT, CW_USEDEFAULT,  CW_USEDEFAULT,
      NULL, NULL, NULL, NULL);
  EXPECT_THAT(host_window_, testing::Truly(::IsWindow));
  RECT client_area = {0};
  ::GetClientRect(host_window_, &client_area);

  ExternalTabSettings s = settings;
  s.parent = host_window_;
  s.dimensions = client_area;

  HWND container_wnd = NULL;
  HWND tab_wnd = NULL;
  scoped_refptr<TabProxy> tab(CreateExternalTab(s, &container_wnd, &tab_wnd));

  EXPECT_TRUE(tab != NULL);
  EXPECT_NE(FALSE, ::IsWindow(container_wnd));
  EXPECT_NE(FALSE, ::IsWindow(tab_wnd));
  return tab;
}

scoped_refptr<TabProxy> ExternalTabUITestMockClient::CreateTabWithUrl(
    const GURL& initial_url) {
  ExternalTabSettings settings =
      ExternalTabUITestMockClient::default_settings;
  settings.initial_url = initial_url;
  return CreateHostWindowAndTab(settings);
}

void ExternalTabUITestMockClient::NavigateInExternalTab(int tab_handle,
    const GURL& url, const GURL& referrer /* = GURL()*/) {
  channel_->ChannelProxy::Send(new AutomationMsg_NavigateInExternalTab(
        tab_handle, url, referrer, NULL));
}

void ExternalTabUITestMockClient::ConnectToExternalTab(gfx::NativeWindow parent,
    const AttachExternalTabParams& attach_params) {
  gfx::NativeWindow tab_container = NULL;
  gfx::NativeWindow tab_window = NULL;
  int tab_handle = 0;
  int session_id = -1;

  IPC::SyncMessage* message = new AutomationMsg_ConnectExternalTab(
      attach_params.cookie, true, NULL, &tab_container, &tab_window,
      &tab_handle, &session_id);
  channel_->Send(message);

  RECT rect;
  ::GetClientRect(parent, &rect);
  Reposition_Params params = {0};
  params.window = tab_container;
  params.flags = SWP_NOZORDER | SWP_SHOWWINDOW;
  params.width = rect.right - rect.left;
  params.height = rect.bottom - rect.top;
  params.set_parent = true;
  params.parent_window = parent;

  channel_->Send(new AutomationMsg_TabReposition(tab_handle, params));
  ::ShowWindow(parent, SW_SHOWNORMAL);
}

void ExternalTabUITestMockClient::NavigateThroughUserGesture() {
  ASSERT_THAT(host_window_, testing::Truly(::IsWindow));
  HWND tab_container = ::GetWindow(host_window_, GW_CHILD);
  ASSERT_THAT(tab_container, testing::Truly(::IsWindow));
  HWND tab = ::GetWindow(tab_container, GW_CHILD);
  ASSERT_THAT(tab, testing::Truly(::IsWindow));
  HWND renderer_window = ::GetWindow(tab, GW_CHILD);
  ASSERT_THAT(renderer_window, testing::Truly(::IsWindow));
  ::SetFocus(renderer_window);
  ::PostMessage(renderer_window, WM_KEYDOWN, VK_TAB, 0);
  ::PostMessage(renderer_window, WM_KEYUP, VK_TAB, 0);
  ::PostMessage(renderer_window, WM_KEYDOWN, VK_RETURN, 0);
  ::PostMessage(renderer_window, WM_KEYUP, VK_RETURN, 0);
}

void ExternalTabUITestMockClient::DestroyHostWindow() {
  ::DestroyWindow(host_window_);
  host_window_ = NULL;
}

bool ExternalTabUITestMockClient::HostWindowExists() {
  return (host_window_ != NULL) && ::IsWindow(host_window_);
}

// Handy macro
#define QUIT_LOOP(loop) testing::InvokeWithoutArgs(\
    CreateFunctor(loop, &TimedMessageLoopRunner::Quit))
#define QUIT_LOOP_SOON(loop, ms) testing::InvokeWithoutArgs(\
    CreateFunctor(loop, &TimedMessageLoopRunner::QuitAfter, ms))

template <typename T> T** ReceivePointer(scoped_ptr<T>& p) {  // NOLINT
  return reinterpret_cast<T**>(&p);
}

template <typename T> T** ReceivePointer(scoped_refptr<T>& p) {  // NOLINT
  return reinterpret_cast<T**>(&p);
}

ExternalTabUITest::ExternalTabUITest() : UITest(MessageLoop::TYPE_UI) {}

// Replace the default automation proxy with our mock client.
ProxyLauncher* ExternalTabUITest::CreateProxyLauncher() {
  return new ExternalTabUITestMockLauncher(&mock_);
}

// Create with specifying a url
// Flaky, http://crbug.com/32293
TEST_F(ExternalTabUITest, FLAKY_CreateExternalTab1) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(mock_,
                &ExternalTabUITestMockClient::DestroyHostWindow));

  EXPECT_CALL(*mock_, HandleClosed(1))
      .Times(1)
      .WillOnce(QUIT_LOOP(&loop));

  tab = mock_->CreateTabWithUrl(GURL(simple_data_url));
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
}

// Create with empty url and then navigate
// Flaky, http://crbug.com/32293
TEST_F(ExternalTabUITest, FLAKY_CreateExternalTab2) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_))
    .Times(1)
    .WillOnce(testing::InvokeWithoutArgs(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow));

  EXPECT_CALL(*mock_, HandleClosed(1))
    .Times(1)
    .WillOnce(QUIT_LOOP(&loop));

  tab = mock_->CreateTabWithUrl(GURL());
  mock_->NavigateInExternalTab(tab->handle(), GURL(simple_data_url));
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
}

// FLAKY: http://crbug.com/60409
TEST_F(ExternalTabUITest, FLAKY_IncognitoMode) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  GURL url("http://anatomyofmelancholy.net");
  std::string cookie = "robert=burton; expires=Thu, 13 Oct 2011 05:04:03 UTC;";

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  ExternalTabSettings incognito =
      ExternalTabUITestMockClient::default_settings;
  incognito.is_off_the_record = true;
  // SetCookie is a sync call and deadlock can happen if window is visible,
  // since it shares same thread with AutomationProxy.
  mock_->host_window_style_ &= ~WS_VISIBLE;
  tab = mock_->CreateHostWindowAndTab(incognito);
  std::string value_result;

  EXPECT_TRUE(tab->SetCookie(url, cookie));
  EXPECT_TRUE(tab->GetCookieByName(url, "robert", &value_result));
  EXPECT_EQ("burton", value_result);
  mock_->DestroyHostWindow();
  CloseBrowserAndServer();
  tab = NULL;

  value_result.clear();
  clear_profile_ = false;
  LaunchBrowserAndServer();
  // SetCookie is a sync call and deadlock can happen if window is visible,
  // since it shares same thread with AutomationProxy.
  mock_->host_window_style_ &= ~WS_VISIBLE;
  tab = mock_->CreateTabWithUrl(GURL());
  EXPECT_TRUE(tab->GetCookieByName(url, "robert", &value_result));
  EXPECT_EQ("", value_result);
  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);
  mock_->DestroyHostWindow();
  CloseBrowserAndServer();
  tab = NULL;
}

// FLAKY: http://crbug.com/44617
TEST_F(ExternalTabUITest, FLAKY_TabPostMessage) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());

  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_)).Times(testing::AnyNumber());

  std::string content =
      "data:text/html,<html><head><script>"
      "function onload() {"
      "  window.externalHost.onmessage = onMessage;"
      "}"
      "function onMessage(evt) {"
      "  window.externalHost.postMessage(evt.data, '*');"
      "}"
      "</script></head>"
      "<body onload='onload()'>external tab test<br></div>"
      "</body></html>";

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(
          ReceivePointer(tab),
          &TabProxy::HandleMessageFromExternalHost,
          std::string("Hello from gtest"),
          std::string("null"), std::string("*"))));

  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(
          testing::StrEq("Hello from gtest"), testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow)),
          QUIT_LOOP_SOON(&loop, 50)));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  tab = mock_->CreateTabWithUrl(GURL(content));
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
}

// Flaky: http://crbug.com/62143
TEST_F(ExternalTabUITest, FLAKY_PostMessageTarget)  {
  net::TestServer test_server(
      net::TestServer::TYPE_HTTP,
      FilePath(FILE_PATH_LITERAL("chrome/test/data/external_tab")));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_)).Times(testing::AnyNumber());

  std::string kTestMessage("Hello from gtest");
  std::string kTestOrigin("http://www.external.tab");

  EXPECT_CALL(*mock_, OnDidNavigate(testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(
          ReceivePointer(tab),
          &TabProxy::HandleMessageFromExternalHost,
          kTestMessage, kTestOrigin, std::string("http://localhost:1337/"))));

  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(
                    testing::StrEq(kTestMessage),
                    testing::_,
                    testing::StrEq(GURL(kTestOrigin).GetOrigin().spec())))
      .Times(1)
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow)),
          QUIT_LOOP_SOON(&loop, 50)));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  ExternalTabSettings s = ExternalTabUITestMockClient::default_settings;
  s.load_requests_via_automation = false;
  s.initial_url = GURL("http://localhost:1337/files/post_message.html");
  tab = mock_->CreateHostWindowAndTab(s);
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
}

// Flaky, http://crbug.com/42545.
TEST_F(ExternalTabUITest, FLAKY_HostNetworkStack) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_)).Times(testing::AnyNumber());

  std::string url = "http://placetogo.org";

  testing::InSequence sequence;
  EXPECT_CALL(*mock_, OnRequestStart(2, testing::AllOf(
          testing::Field(&AutomationURLRequest::url, StrEq(url + "/")),
          testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      // We can simply do CreateFunctor(1, 2, &http_200) since we know the
      // tab handle and request id, but using WithArgs<> is much more fancy :)
      .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyStarted,
          &ExternalTabUITestMockClient::http_200))));

  // Return some trivial page, that have a link to a "logo.gif" image
  const std::string data = "<!DOCTYPE html><title>Hello</title>"
                           "<img src=\"logo.gif\">";

  EXPECT_CALL(*mock_, OnRequestRead(2, testing::Gt(0)))
      .Times(2)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyData, &data, 1, 2)))
      .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyEOF))));

  // Expect navigation is ok.
  EXPECT_CALL(*mock_, OnDidNavigate(testing::Field(&NavigationInfo::url,
                                                   GURL(url))))
      .Times(1);

  // Expect GET request for logo.gif
  EXPECT_CALL(*mock_, OnRequestStart(3, testing::AllOf(
      testing::Field(&AutomationURLRequest::url, StrEq(url + "/logo.gif")),
      testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::Reply404, 1, 3)));

  EXPECT_CALL(*mock_, OnRequestRead(3, testing::Gt(0)))
    .Times(1);

  // Chrome makes a brave request for favicon.ico
  EXPECT_CALL(*mock_, OnRequestStart(4, testing::AllOf(
      testing::Field(&AutomationURLRequest::url,
                     StrEq(url + "/favicon.ico")),
      testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::Reply404, 1, 4)),
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow))));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  EXPECT_CALL(*mock_, OnRequestRead(4, testing::Gt(0)))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(&loop, 300));

  tab = mock_->CreateTabWithUrl(GURL(url));
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
}

// Flaky, http://crbug.com/61023.
TEST_F(ExternalTabUITest, FLAKY_HostNetworkStackAbortRequest) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());

  std::string url = "http://placetogo.org";

  testing::InSequence sequence;
  EXPECT_CALL(*mock_, OnRequestStart(2, testing::AllOf(
          testing::Field(&AutomationURLRequest::url, StrEq(url + "/")),
          testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      // We can simply do CreateFunctor(1, 2, &http_200) since we know the
      // tab handle and request id, but using WithArgs<> is much more fancy :)
      .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyStarted,
          &ExternalTabUITestMockClient::http_200))));

  // Return some trivial page, that have a link to a "logo.gif" image
  const std::string data = "<!DOCTYPE html><title>Hello";

  EXPECT_CALL(*mock_, OnRequestRead(2, testing::Gt(0)))
      .Times(2)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyData, &data, 1, 2)))
      .WillOnce(testing::WithArgs<0, 0>(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::DestroyHostWindow))));

  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  EXPECT_CALL(*mock_, OnRequestEnd(2, testing::_))
      .Times(1)
      .WillOnce(QUIT_LOOP_SOON(&loop, 300));

  tab = mock_->CreateTabWithUrl(GURL(url));
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
}

// Flaky, http://crbug.com/61023.
TEST_F(ExternalTabUITest, FLAKY_HostNetworkStackUnresponsiveRenderer) {
  scoped_refptr<TabProxy> tab;
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_)).Times(testing::AnyNumber());

  std::string url = "http://placetogo.org";

  EXPECT_CALL(*mock_, OnRequestStart(3, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnDidNavigate(_)).Times(testing::AnyNumber());

  testing::InSequence sequence;
  EXPECT_CALL(*mock_, OnRequestStart(2, testing::AllOf(
          testing::Field(&AutomationURLRequest::url, StrEq(url + "/")),
          testing::Field(&AutomationURLRequest::method, StrEq("GET")))))
      .Times(1)
      // We can simply do CreateFunctor(1, 2, &http_200) since we know the
      // tab handle and request id, but using WithArgs<> is much more fancy :)
      .WillOnce(testing::WithArgs<0, 0>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyStarted,
          &ExternalTabUITestMockClient::http_200))));

  const std::string head = "<html><title>Hello</title><body>";

  const std::string data = "<table border=\"1\"><tr><th>Month</th>"
                           "<th>Savings</th></tr><tr><td>January</td>"
                           "<td>$100</td></tr><tr><td>February</td>"
                           "<td>$100</td></tr><tr><td>March</td>"
                           "<td>$100</td></tr><tr><td>April</td>"
                           "<td>$100</td></tr><tr><td>May</td>"
                           "<td>$100</td></tr><tr><td>June</td>"
                           "<td>$100</td></tr><tr><td>July</td>"
                           "<td>$100</td></tr><tr><td>Aug</td>"
                           "<td>$100</td></tr><tr><td>Sept</td>"
                           "<td>$100</td></tr><tr><td>Oct</td>"
                           "<td>$100</td></tr><tr><td>Nov</td>"
                           "<td>$100</td></tr><tr><td>Dec</td>"
                           "<td>$100</td></tr></table>";

  const std::string tail = "</body></html>";

  EXPECT_CALL(*mock_, OnRequestRead(2, testing::Gt(0)))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ReplyData, &head, 1, 2)));

  EXPECT_CALL(*mock_, OnRequestRead(2, testing::Gt(0)))
      .Times(100)
      .WillRepeatedly(testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::ReplyData, &data, 1, 2)));

  EXPECT_CALL(*mock_, OnRequestRead(2, testing::Gt(0)))
      .Times(testing::AnyNumber())
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::ReplyData, &tail, 1, 2)),
          testing::InvokeWithoutArgs(CreateFunctor(mock_,
              &ExternalTabUITestMockClient::ReplyEOF, 1, 2)),
          QUIT_LOOP_SOON(&loop, 300)));
  EXPECT_CALL(*mock_, HandleClosed(1)).Times(1);

  tab = mock_->CreateTabWithUrl(GURL(url));
  loop.RunFor(TestTimeouts::action_max_timeout_ms());
  mock_->DestroyHostWindow();
}

class ExternalTabUITestPopupEnabled : public ExternalTabUITest {
 public:
  ExternalTabUITestPopupEnabled() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

#if defined(OS_WIN)
// http://crbug.com/61023 - Fails on one popular operating system.
#define MAYBE_WindowDotOpen FLAKY_WindowDotOpen
#define MAYBE_UserGestureTargetBlank FLAKY_UserGestureTargetBlank
#else
#define MAYBE_WindowDotOpen WindowDotOpen
#define MAYBE_UserGestureTargetBlank UserGestureTargetBlank
#endif

// Testing AutomationMsg_AttachExternalTab callback from Chrome.
// Open a popup window with window.open() call. The created popup window opens
// another popup window (again using window.open() call).
TEST_F(ExternalTabUITestPopupEnabled, MAYBE_WindowDotOpen) {
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  mock_->IgnoreFavIconNetworkRequest();
  // Ignore navigation state changes.
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnDidNavigate(_)).Times(testing::AnyNumber());

  GURL main_url("http://placetogo.com/");
  std::string main_html =
    "<html><head><script type='text/javascript' language='JavaScript'>"
    "window.open('popup1.html','','toolbar=no,menubar=no,location=yes,"
    "height=320,width=300,left=1');"
    "</script></head><body>Main.</body></html>";
  mock_->ServeHTMLData(1, main_url, main_html);
  EXPECT_CALL(*mock_, OnLoad(_)).Times(1);

  GURL popup1_url("http://placetogo.com/popup1.html");
  std::string popup1_html =
    "<html><head><script type='text/javascript' language='JavaScript'>"
    "window.open('popup2.html','','');"
    "</script></head><body>Popup1.</body></html>";
  mock_->ServeHTMLData(2, popup1_url, popup1_html);
  EXPECT_CALL(*mock_, OnLoad(_)).Times(1);

  GURL popup2_url("http://placetogo.com/popup2.html");
  std::string popup2_html = "<html><body>Popup2.</body></html>";
  mock_->ServeHTMLData(3, popup2_url, popup2_html);
  EXPECT_CALL(*mock_, OnLoad(_)).Times(1)
      .WillOnce(QUIT_LOOP_SOON(&loop, 500));

  DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  HWND popup1_host = CreateWindowW(L"Button", L"popup1_host", style,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      NULL, NULL, NULL, NULL);

  HWND popup2_host = CreateWindowW(L"Button", L"popup2_host", style,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,  CW_USEDEFAULT,
      NULL, NULL, NULL, NULL);

  EXPECT_CALL(*mock_, OnAttachExternalTab(_))
      .Times(1)
      .WillOnce(testing::WithArgs<0>(testing::Invoke(CreateFunctor(mock_,
          &ExternalTabUITestMockClient::ConnectToExternalTab, popup1_host))));

  EXPECT_CALL(*mock_, OnAttachExternalTab(_))
    .Times(1)
    .WillOnce(testing::WithArgs<0>(testing::Invoke(CreateFunctor(mock_,
        &ExternalTabUITestMockClient::ConnectToExternalTab, popup2_host))));

  mock_->CreateTabWithUrl(main_url);

  loop.RunFor(TestTimeouts::action_max_timeout_ms());

  EXPECT_CALL(*mock_, HandleClosed(1));
  EXPECT_CALL(*mock_, HandleClosed(2));
  EXPECT_CALL(*mock_, HandleClosed(3));

  mock_->DestroyHostWindow();
  ::DestroyWindow(popup1_host);
  ::DestroyWindow(popup2_host);
}

// Open a new window by simulating a user gesture through keyboard.
TEST_F(ExternalTabUITestPopupEnabled, MAYBE_UserGestureTargetBlank) {
  TimedMessageLoopRunner loop(MessageLoop::current());
  ASSERT_THAT(mock_, testing::NotNull());
  mock_->IgnoreFavIconNetworkRequest();
  // Ignore navigation state changes.
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnDidNavigate(_)).Times(testing::AnyNumber());

  GURL main_url("http://placetogo.com/");
  std::string main_html = "<!DOCTYPE html><title>Hello</title>"
      "<a href='http://foo.com/' target='_blank'>Link</a>";
  mock_->ServeHTMLData(1, main_url, main_html);

  HWND foo_host = CreateWindowW(L"Button", L"foo_host",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
      CW_USEDEFAULT,  CW_USEDEFAULT, NULL, NULL, NULL, NULL);

  testing::InSequence s;
  EXPECT_CALL(*mock_, OnLoad(_))
      .WillOnce(testing::InvokeWithoutArgs(testing::CreateFunctor(mock_,
          &ExternalTabUITestMockClient::NavigateThroughUserGesture)));

  EXPECT_CALL(*mock_, OnAttachExternalTab(_))
      .Times(1)
      .WillOnce(QUIT_LOOP_SOON(&loop, 500));

  mock_->CreateTabWithUrl(main_url);
  loop.RunFor(TestTimeouts::action_max_timeout_ms());

  EXPECT_CALL(*mock_, HandleClosed(1));
  ::DestroyWindow(foo_host);
  mock_->DestroyHostWindow();
}

#endif  // defined(OS_WIN)

// TODO(port): Need to port autocomplete_edit_proxy.* first.
#if defined(OS_WIN) || defined(OS_LINUX)
TEST_F(AutomationProxyTest, AutocompleteGetSetText) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<AutocompleteEditProxy> edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(edit->is_valid());
  const string16 text_to_set = ASCIIToUTF16("Lollerskates");
  string16 actual_text;
  EXPECT_TRUE(edit->SetText(text_to_set));
  EXPECT_TRUE(edit->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
  scoped_refptr<AutocompleteEditProxy> edit2(
      browser->GetAutocompleteEdit());
  EXPECT_TRUE(edit2->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
}

TEST_F(AutomationProxyTest, AutocompleteParallelProxy) {
  scoped_refptr<BrowserProxy> browser1(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser1.get());
  scoped_refptr<AutocompleteEditProxy> edit1(
      browser1->GetAutocompleteEdit());
  ASSERT_TRUE(edit1.get());
  EXPECT_TRUE(browser1->RunCommand(IDC_NEW_WINDOW));
  scoped_refptr<BrowserProxy> browser2(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser2.get());
  scoped_refptr<AutocompleteEditProxy> edit2(
      browser2->GetAutocompleteEdit());
  ASSERT_TRUE(edit2.get());
  EXPECT_TRUE(browser2->GetTab(0)->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));
  const string16 text_to_set1 = ASCIIToUTF16("Lollerskates");
  const string16 text_to_set2 = ASCIIToUTF16("Roflcopter");
  string16 actual_text1, actual_text2;
  EXPECT_TRUE(edit1->SetText(text_to_set1));
  EXPECT_TRUE(edit2->SetText(text_to_set2));
  EXPECT_TRUE(edit1->GetText(&actual_text1));
  EXPECT_TRUE(edit2->GetText(&actual_text2));
  EXPECT_EQ(text_to_set1, actual_text1);
  EXPECT_EQ(text_to_set2, actual_text2);
}

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_MACOSX)
// Disabled, http://crbug.com/48601.
#define AutocompleteMatchesTest DISABLED_AutocompleteMatchesTest
#else
// Flaky, http://crbug.com/19876.
#define AutocompleteMatchesTest FLAKY_AutocompleteMatchesTest
#endif
TEST_F(AutomationProxyVisibleTest, AutocompleteMatchesTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<AutocompleteEditProxy> edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(edit->is_valid());
  EXPECT_TRUE(browser->ApplyAccelerator(IDC_FOCUS_LOCATION));
  ASSERT_TRUE(edit->WaitForFocus());
  EXPECT_TRUE(edit->SetText(ASCIIToUTF16("Roflcopter")));
  EXPECT_TRUE(edit->WaitForQuery(TestTimeouts::action_max_timeout_ms()));
  bool query_in_progress;
  EXPECT_TRUE(edit->IsQueryInProgress(&query_in_progress));
  EXPECT_FALSE(query_in_progress);
  std::vector<AutocompleteMatchData> matches;
  EXPECT_TRUE(edit->GetAutocompleteMatches(&matches));
  EXPECT_FALSE(matches.empty());
}

// Flaky especially on Windows. See crbug.com/25039.
TEST_F(AutomationProxyTest, FLAKY_AppModalDialogTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool modal_dialog_showing = false;
  ui::MessageBoxFlags::DialogButton button =
      ui::MessageBoxFlags::DIALOGBUTTON_NONE;
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_EQ(ui::MessageBoxFlags::DIALOGBUTTON_NONE, button);

  // Show a simple alert.
  std::string content =
      "data:text/html,<html><head><script>function onload() {"
      "setTimeout(\"alert('hello');\", 1000); }</script></head>"
      "<body onload='onload()'></body></html>";
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(content)));
  EXPECT_TRUE(automation()->WaitForAppModalDialog());
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(ui::MessageBoxFlags::DIALOGBUTTON_OK, button);

  // Test that clicking missing button fails graciously and does not close the
  // dialog.
  EXPECT_FALSE(automation()->ClickAppModalDialogButton(
      ui::MessageBoxFlags::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);

  // Now click OK, that should close the dialog.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      ui::MessageBoxFlags::DIALOGBUTTON_OK));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);

  // Show a confirm dialog.
  content =
      "data:text/html,<html><head><script>var result = -1; function onload() {"
      "setTimeout(\"result = confirm('hello') ? 0 : 1;\", 1000);} </script>"
      "</head><body onload='onload()'></body></html>";
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(content)));
  EXPECT_TRUE(automation()->WaitForAppModalDialog());
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(ui::MessageBoxFlags::DIALOGBUTTON_OK |
            ui::MessageBoxFlags::DIALOGBUTTON_CANCEL, button);

  // Click OK.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      ui::MessageBoxFlags::DIALOGBUTTON_OK));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  int result = -1;
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      std::wstring(),
      L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(0, result);

  // Try again.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(content)));
  EXPECT_TRUE(automation()->WaitForAppModalDialog());
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(ui::MessageBoxFlags::DIALOGBUTTON_OK |
            ui::MessageBoxFlags::DIALOGBUTTON_CANCEL, button);

  // Click Cancel this time.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      ui::MessageBoxFlags::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      std::wstring(),
      L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(1, result);
}

class AutomationProxyTest5 : public UITest {
 protected:
  AutomationProxyTest5() {
    show_window_ = true;
    dom_automation_enabled_ = true;
    // We need to disable popup blocking to ensure that the RenderView
    // instance for the popup actually closes.
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

TEST_F(AutomationProxyTest5, TestLifetimeOfDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("dom_automation_test_with_popup.html");

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(filename)));

  // Allow some time for the popup to show up and close.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());

  std::wstring expected(L"string");
  std::wstring jscript = CreateJSString(L"\"" + expected + L"\"");
  std::wstring actual;
  ASSERT_TRUE(tab->ExecuteAndExtractString(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

class AutomationProxySnapshotTest : public UITest {
 protected:
  AutomationProxySnapshotTest() {
    dom_automation_enabled_ = true;
    if (!snapshot_dir_.CreateUniqueTempDir())
      ADD_FAILURE() << "Unable to create temporary directory";
    else
      snapshot_path_ = snapshot_dir_.path().AppendASCII("snapshot.png");
  }

  // Asserts that the given png file can be read and decoded into the given
  // bitmap.
  void AssertReadPNG(const FilePath& filename, SkBitmap* bitmap) {
    DCHECK(bitmap);
    ASSERT_TRUE(file_util::PathExists(filename));

    int64 size64;
    ASSERT_TRUE(file_util::GetFileSize(filename, &size64));
    // Check that the file is not too big to read in (less than 100 MB).
    ASSERT_LT(size64, 1024 * 1024 * 100);

    // Read and decode image.
    int size = static_cast<int>(size64);
    scoped_array<char> data(new char[size]);
    int bytes_read = file_util::ReadFile(filename, &data[0], size);
    ASSERT_EQ(size, bytes_read);
    ASSERT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<unsigned char*>(&data[0]),
        bytes_read,
        bitmap));
  }

  // Returns the file path for the directory for these tests appended with
  // the given relative path.
  FilePath GetTestFilePath(const char* relative_path) {
    FilePath filename(test_data_directory_);
    return filename.AppendASCII("automation_proxy_snapshot")
        .AppendASCII(relative_path);
  }

  FilePath snapshot_path_;
  ScopedTempDir snapshot_dir_;
};

// See http://crbug.com/63022.
#if defined(OS_LINUX)
#define MAYBE_ContentSmallerThanView FAILS_ContentSmallerThanView
#else
#define MAYBE_ContentSmallerThanView ContentSmallerThanView
#endif
// Tests that taking a snapshot when the content is smaller than the view
// produces a snapshot equal to the view size.
TEST_F(AutomationProxySnapshotTest, MAYBE_ContentSmallerThanView) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SetBounds(gfx::Rect(300, 400)));

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(chrome::kAboutBlankURL)));

  gfx::Rect view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &view_bounds,
                                    false));

  ASSERT_TRUE(tab->CaptureEntirePageAsPNG(snapshot_path_));

  SkBitmap bitmap;
  ASSERT_NO_FATAL_FAILURE(AssertReadPNG(snapshot_path_, &bitmap));
  ASSERT_EQ(view_bounds.width(), bitmap.width());
  ASSERT_EQ(view_bounds.height(), bitmap.height());
}

// See http://crbug.com/63022.
#if defined(OS_LINUX)
#define MAYBE_ContentLargerThanView FAILS_ContentLargerThanView
#else
#define MAYBE_ContentLargerThanView ContentLargerThanView
#endif
// Tests that taking a snapshot when the content is larger than the view
// produces a snapshot equal to the content size.
TEST_F(AutomationProxySnapshotTest, MAYBE_ContentLargerThanView) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  // Resize the window to guarantee that the content is larger than the view.
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SetBounds(gfx::Rect(300, 400)));

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath set_size_page = GetTestFilePath("set_size.html?600,800");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(set_size_page)));

  ASSERT_TRUE(tab->CaptureEntirePageAsPNG(snapshot_path_));

  SkBitmap bitmap;
  ASSERT_NO_FATAL_FAILURE(AssertReadPNG(snapshot_path_, &bitmap));
  ASSERT_EQ(600, bitmap.width());
  ASSERT_EQ(800, bitmap.height());
}

// Tests taking a large snapshot works.
TEST_F(AutomationProxySnapshotTest, LargeSnapshot) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // 2000x2000 creates an approximately 15 MB bitmap.
  // Don't increase this too much. At least my linux box has SHMMAX set at
  // 32 MB.
  FilePath set_size_page = GetTestFilePath("set_size.html?2000,2000");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(set_size_page)));

  ASSERT_TRUE(tab->CaptureEntirePageAsPNG(snapshot_path_));

  SkBitmap bitmap;
  ASSERT_NO_FATAL_FAILURE(AssertReadPNG(snapshot_path_, &bitmap));
  ASSERT_EQ(2000, bitmap.width());
  ASSERT_EQ(2000, bitmap.height());
}

#if defined(OS_MACOSX)
// Most pixels on mac are slightly off.
#define MAYBE_ContentsCorrect DISABLED_ContentsCorrect
#elif defined(OS_LINUX)
// See http://crbug.com/63022.
#define MAYBE_ContentsCorrect FAILS_ContentsCorrect
#else
#define MAYBE_ContentsCorrect ContentsCorrect
#endif

// Tests that the snapshot contents are correct.
TEST_F(AutomationProxySnapshotTest, MAYBE_ContentsCorrect) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  const gfx::Size img_size(400, 300);
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SetBounds(gfx::Rect(img_size)));

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath set_size_page = GetTestFilePath("just_image.html");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(set_size_page)));

  ASSERT_TRUE(tab->CaptureEntirePageAsPNG(snapshot_path_));

  SkBitmap snapshot_bmp;
  ASSERT_NO_FATAL_FAILURE(AssertReadPNG(snapshot_path_, &snapshot_bmp));
  ASSERT_EQ(img_size.width(), snapshot_bmp.width());
  ASSERT_EQ(img_size.height(), snapshot_bmp.height());

  SkBitmap reference_bmp;
  ASSERT_NO_FATAL_FAILURE(AssertReadPNG(GetTestFilePath("image.png"),
                                        &reference_bmp));
  ASSERT_EQ(img_size.width(), reference_bmp.width());
  ASSERT_EQ(img_size.height(), reference_bmp.height());

  SkAutoLockPixels lock_snapshot(snapshot_bmp);
  SkAutoLockPixels lock_reference(reference_bmp);
  int diff_pixels_count = 0;
  for (int x = 0; x < img_size.width(); ++x) {
    for (int y = 0; y < img_size.height(); ++y) {
      if (*snapshot_bmp.getAddr32(x, y) != *reference_bmp.getAddr32(x, y)) {
        ++diff_pixels_count;
      }
    }
  }
  ASSERT_EQ(diff_pixels_count, 0);
}
