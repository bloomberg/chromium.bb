// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_switches.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

namespace content {
namespace {

void SetUrlRequestMock(const FilePath& path) {
  URLRequestMockHTTPJob::AddUrlHandler(path);
}

}

class PluginTest : public ContentBrowserTest {
 protected:
  PluginTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // Some NPAPI tests schedule garbage collection to force object tear-down.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose_gc");

#if defined(OS_WIN)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "MediaPlayerNew") == 0) {
      // The installer adds our process names to the registry key below.  Since
      // the installer might not have run on this machine, add it manually.
      base::win::RegKey regkey;
      if (regkey.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList",
                      KEY_WRITE) == ERROR_SUCCESS) {
        regkey.CreateKey(L"BROWSER_TESTS.EXE", KEY_READ);
      }
    } else if (strcmp(test_info->name(), "MediaPlayerOld") == 0) {
      // When testing the old WMP plugin, we need to force Chrome to not load
      // the new plugin.
      command_line->AppendSwitch(switches::kUseOldWMPPlugin);
    } else if (strcmp(test_info->name(), "FlashSecurity") == 0) {
      command_line->AppendSwitchASCII(switches::kTestSandbox,
                                      "security_tests.dll");
    }
#elif defined(OS_MACOSX)
    FilePath plugin_dir;
    PathService::Get(base::DIR_MODULE, &plugin_dir);
    plugin_dir = plugin_dir.AppendASCII("plugins");
    // The plugins directory isn't read by default on the Mac, so it needs to be
    // explicitly registered.
    command_line->AppendSwitchPath(switches::kExtraPluginDir, plugin_dir);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    FilePath path = GetTestFilePath("", "");
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(&SetUrlRequestMock, path));
  }

  static void LoadAndWaitInWindow(Shell* window, const GURL& url) {
    string16 expected_title(ASCIIToUTF16("OK"));
    TitleWatcher title_watcher(window->web_contents(), expected_title);
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("plugin_not_found"));
    NavigateToURL(window, url);
    string16 title = title_watcher.WaitAndGetTitle();
    if (title == ASCIIToUTF16("plugin_not_found")) {
      const testing::TestInfo* const test_info =
          testing::UnitTest::GetInstance()->current_test_info();
      LOG(INFO) << "PluginTest." << test_info->name() <<
                   " not running because plugin not installed.";
    } else {
      EXPECT_EQ(expected_title, title);
    }
  }

  void LoadAndWait(const GURL& url) {
    LoadAndWaitInWindow(shell(), url);
  }

  GURL GetURL(const char* filename) {
    return GetTestUrl("npapi", filename);
  }

  void NavigateAway() {
    GURL url = GetTestUrl("", "simple_page.html");
    LoadAndWait(url);
  }

  void TestPlugin(const char* filename) {
    FilePath path = GetTestFilePath("plugin", filename);
    if (!file_util::PathExists(path)) {
      const testing::TestInfo* const test_info =
          testing::UnitTest::GetInstance()->current_test_info();
      LOG(INFO) << "PluginTest." << test_info->name() <<
                   " not running because test data wasn't found.";
      return;
    }

    GURL url = GetTestUrl("plugin", filename);
    LoadAndWait(url);
  }
};

// Make sure that navigating away from a plugin referenced by JS doesn't
// crash.
IN_PROC_BROWSER_TEST_F(PluginTest, UnloadNoCrash) {
  LoadAndWait(GetURL("layout_test_plugin.html"));
  NavigateAway();
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
// Flaky: http://crbug.com/59327
IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeletePluginGetUrl) {
  LoadAndWait(GetURL("self_delete_plugin_geturl.html"));
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
// Flaky. See http://crbug.com/30702
IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeletePluginInvoke) {
  LoadAndWait(GetURL("self_delete_plugin_invoke.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, NPObjectReleasedOnDestruction) {
  NavigateToURL(shell(), GetURL("npobject_released_on_destruction.html"));
  NavigateAway();
}

// Test that a dialog is properly created when a plugin throws an
// exception.  Should be run for in and out of process plugins, but
// the more interesting case is out of process, where we must route
// the exception to the correct renderer.
IN_PROC_BROWSER_TEST_F(PluginTest, NPObjectSetException) {
  LoadAndWait(GetURL("npobject_set_exception.html"));
}

#if defined(OS_WIN)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous mouseup works correctly.
// This was never ported to Mac. The only thing remaining is to make
// SimulateMouseClick get to Mac plugins, currently it doesn't work.
IN_PROC_BROWSER_TEST_F(PluginTest,
                       SelfDeletePluginInvokeInSynchronousMouseUp) {
  NavigateToURL(shell(), GetURL("execute_script_delete_in_mouse_up.html"));

  string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
  SimulateMouseClick(shell()->web_contents(), 0,
      WebKit::WebMouseEvent::ButtonLeft);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
#endif

// Flaky, http://crbug.com/60071.
IN_PROC_BROWSER_TEST_F(PluginTest, GetURLRequest404Response) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath().AppendASCII("npapi").
                 AppendASCII("plugin_url_request_404.html")));
  LoadAndWait(url);
}

// Tests if a plugin executing a self deleting script using Invoke with
// a modal dialog showing works without crashing or hanging
// Disabled, flakily exceeds timeout, http://crbug.com/46257.
IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeletePluginInvokeAlert) {
  // Navigate asynchronously because if we waitd until it completes, there's a
  // race condition where the alert can come up before we start watching for it.
  shell()->LoadURL(GetURL("self_delete_plugin_invoke_alert.html"));

  string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  WaitForAppModalDialog(shell());

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test passing arguments to a plugin.
IN_PROC_BROWSER_TEST_F(PluginTest, Arguments) {
  LoadAndWait(GetURL("arguments.html"));
}

// Test invoking many plugins within a single page.
IN_PROC_BROWSER_TEST_F(PluginTest, ManyPlugins) {
  LoadAndWait(GetURL("many_plugins.html"));
}

// Test various calls to GetURL from a plugin.
IN_PROC_BROWSER_TEST_F(PluginTest, GetURL) {
  LoadAndWait(GetURL("geturl.html"));
}

// Test various calls to GetURL for javascript URLs with
// non NULL targets from a plugin.
IN_PROC_BROWSER_TEST_F(PluginTest, GetJavaScriptURL) {
  LoadAndWait(GetURL("get_javascript_url.html"));
}

// Test that calling GetURL with a javascript URL and target=_self
// works properly when the plugin is embedded in a subframe.
IN_PROC_BROWSER_TEST_F(PluginTest, GetJavaScriptURL2) {
  LoadAndWait(GetURL("get_javascript_url2.html"));
}

// Test is flaky on linux/cros/win builders.  http://crbug.com/71904
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_GetURLRedirectNotification) {
  LoadAndWait(GetURL("geturl_redirect_notify.html"));
}

// Tests that identity is preserved for NPObjects passed from a plugin
// into JavaScript.
IN_PROC_BROWSER_TEST_F(PluginTest, NPObjectIdentity) {
  LoadAndWait(GetURL("npobject_identity.html"));
}

// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
IN_PROC_BROWSER_TEST_F(PluginTest, NPObjectProxy) {
  LoadAndWait(GetURL("npobject_proxy.html"));
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Tests if a plugin executing a self deleting script in the context of
// a synchronous paint event works correctly
// http://crbug.com/44960
IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeletePluginInvokeInSynchronousPaint) {
  LoadAndWait(GetURL("execute_script_delete_in_paint.html"));
}
#endif

// Tests that if a plugin executes a self resizing script in the context of a
// synchronous paint, the plugin doesn't use deallocated memory.
// http://crbug.com/139462
IN_PROC_BROWSER_TEST_F(PluginTest, ResizeDuringPaint) {
  LoadAndWait(GetURL("resize_during_paint.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeletePluginInNewStream) {
  LoadAndWait(GetURL("self_delete_plugin_stream.html"));
}

// This test asserts on Mac in plugin_host in the NPNVWindowNPObject case.
#if !(defined(OS_MACOSX) && !defined(NDEBUG))
// If this test flakes use http://crbug.com/95558.
IN_PROC_BROWSER_TEST_F(PluginTest, DeletePluginInDeallocate) {
  LoadAndWait(GetURL("plugin_delete_in_deallocate.html"));
}
#endif

#if defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(PluginTest, VerifyPluginWindowRect) {
  LoadAndWait(GetURL("verify_plugin_window_rect.html"));
}

// Tests that creating a new instance of a plugin while another one is handling
// a paint message doesn't cause deadlock.
IN_PROC_BROWSER_TEST_F(PluginTest, CreateInstanceInPaint) {
  LoadAndWait(GetURL("create_instance_in_paint.html"));
}

// Tests that putting up an alert in response to a paint doesn't deadlock.
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_AlertInWindowMessage) {
  NavigateToURL(shell(), GetURL("alert_in_window_message.html"));

  WaitForAppModalDialog(shell());
  WaitForAppModalDialog(shell());
}

IN_PROC_BROWSER_TEST_F(PluginTest, VerifyNPObjectLifetimeTest) {
  LoadAndWait(GetURL("npobject_lifetime_test.html"));
}

// Tests that we don't crash or assert if NPP_New fails
IN_PROC_BROWSER_TEST_F(PluginTest, NewFails) {
  LoadAndWait(GetURL("new_fails.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeletePluginInNPNEvaluate) {
  LoadAndWait(GetURL("execute_script_delete_in_npn_evaluate.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, SelfDeleteCreatePluginInNPNEvaluate) {
  LoadAndWait(GetURL("npn_plugin_delete_create_in_evaluate.html"));
}

#endif  // OS_WIN

// If this flakes, reopen http://crbug.com/17645
// As of 6 July 2011, this test is flaky on Windows (perhaps due to timing out).
#if !defined(OS_MACOSX)
// Disabled on Mac because the plugin side isn't implemented yet, see
// "TODO(port)" in plugin_javascript_open_popup.cc.
IN_PROC_BROWSER_TEST_F(PluginTest, OpenPopupWindowWithPlugin) {
  LoadAndWait(GetURL("get_javascript_open_popup_with_plugin.html"));
}
#endif

// Test checking the privacy mode is off.
IN_PROC_BROWSER_TEST_F(PluginTest, PrivateDisabled) {
  LoadAndWait(GetURL("private.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, ScheduleTimer) {
  LoadAndWait(GetURL("schedule_timer.html"));
}

IN_PROC_BROWSER_TEST_F(PluginTest, PluginThreadAsyncCall) {
  LoadAndWait(GetURL("plugin_thread_async_call.html"));
}

// Test checking the privacy mode is on.
// If this flakes on Linux, use http://crbug.com/104380
IN_PROC_BROWSER_TEST_F(PluginTest, PrivateEnabled) {
  GURL url = GetURL("private.html");
  url = GURL(url.spec() + "?private");
  LoadAndWaitInWindow(CreateOffTheRecordBrowser(), url);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test a browser hang due to special case of multiple
// plugin instances indulged in sync calls across renderer.
IN_PROC_BROWSER_TEST_F(PluginTest, MultipleInstancesSyncCalls) {
  LoadAndWait(GetURL("multiple_instances_sync_calls.html"));
}
#endif

IN_PROC_BROWSER_TEST_F(PluginTest, GetURLRequestFailWrite) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath().AppendASCII("npapi").
                 AppendASCII("plugin_url_request_fail_write.html")));
  LoadAndWait(url);
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(PluginTest, EnsureScriptingWorksInDestroy) {
  LoadAndWait(GetURL("ensure_scripting_works_in_destroy.html"));
}

// This test uses a Windows Event to signal to the plugin that it should crash
// on NP_Initialize.
IN_PROC_BROWSER_TEST_F(PluginTest, NoHangIfInitCrashes) {
  HANDLE crash_event = CreateEvent(NULL, TRUE, FALSE, L"TestPluginCrashOnInit");
  SetEvent(crash_event);
  LoadAndWait(GetURL("no_hang_if_init_crashes.html"));
  CloseHandle(crash_event);
}
#endif

// If this flakes on Mac, use http://crbug.com/111508
IN_PROC_BROWSER_TEST_F(PluginTest, PluginReferrerTest) {
  GURL url(URLRequestMockHTTPJob::GetMockUrl(
      FilePath().AppendASCII("npapi").
                 AppendASCII("plugin_url_request_referrer_test.html")));
  LoadAndWait(url);
}

#if defined(OS_MACOSX)
// Test is flaky, see http://crbug.com/134515.
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_PluginConvertPointTest) {
  gfx::Rect bounds(50, 50, 400, 400);
  SetWindowBounds(shell()->window(), bounds);

  NavigateToURL(shell(), GetURL("convert_point.html"));

  string16 expected_title(ASCIIToUTF16("OK"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
  // TODO(stuartmorgan): When the automation system supports sending clicks,
  // change the test to trigger on mouse-down rather than window focus.

  // TODO: is this code still needed? It was here when it used to run in
  // browser_tests.
  //static_cast<WebContentsDelegate*>(shell())->
  //    ActivateContents(shell()->web_contents());
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
#endif

IN_PROC_BROWSER_TEST_F(PluginTest, Flash) {
  TestPlugin("flash.html");
}

#if defined(OS_WIN)
// Windows only test
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_FlashSecurity) {
  TestPlugin("flash.html");
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
// TODO(port) Port the following tests to platforms that have the required
// plugins.
// Flaky: http://crbug.com/55915
IN_PROC_BROWSER_TEST_F(PluginTest, Quicktime) {
  TestPlugin("quicktime.html");
}

// Disabled - http://crbug.com/44662
IN_PROC_BROWSER_TEST_F(PluginTest, MediaPlayerNew) {
  TestPlugin("wmp_new.html");
}

// http://crbug.com/4809
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_MediaPlayerOld) {
  TestPlugin("wmp_old.html");
}

// Disabled - http://crbug.com/44673
IN_PROC_BROWSER_TEST_F(PluginTest, Real) {
  TestPlugin("real.html");
}

IN_PROC_BROWSER_TEST_F(PluginTest, FlashOctetStream) {
  TestPlugin("flash-octet-stream.html");
}

#if defined(OS_WIN)
// http://crbug.com/53926
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_FlashLayoutWhilePainting) {
#else
IN_PROC_BROWSER_TEST_F(PluginTest, FlashLayoutWhilePainting) {
#endif
  TestPlugin("flash-layout-while-painting.html");
}

// http://crbug.com/8690
IN_PROC_BROWSER_TEST_F(PluginTest, DISABLED_Java) {
  TestPlugin("Java.html");
}

IN_PROC_BROWSER_TEST_F(PluginTest, Silverlight) {
  TestPlugin("silverlight.html");
}
#endif  // defined(OS_WIN)

}  // namespace content
