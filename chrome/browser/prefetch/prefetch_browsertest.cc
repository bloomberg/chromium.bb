// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/prefetch_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"

using chrome_browser_net::NetworkPredictionOptions;
using content::BrowserThread;
using net::NetworkChangeNotifier;

namespace {

const char kPrefetchPage[] = "files/prerender/simple_prefetch.html";

class MockNetworkChangeNotifierWIFI : public NetworkChangeNotifier {
 public:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return NetworkChangeNotifier::CONNECTION_WIFI;
  }
};

class MockNetworkChangeNotifier4G : public NetworkChangeNotifier {
 public:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return NetworkChangeNotifier::CONNECTION_4G;
  }
};

class PrefetchBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit PrefetchBrowserTestBase(bool disabled_via_field_trial)
      : disabled_via_field_trial_(disabled_via_field_trial) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (disabled_via_field_trial_) {
      command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                      "Prefetch/ExperimentDisabled/");
    }
  }

  void SetPreference(NetworkPredictionOptions value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kNetworkPredictionOptions, value);
  }

  bool RunPrefetchExperiment(bool expect_success, Browser* browser) {
    GURL url = test_server()->GetURL(kPrefetchPage);

    const base::string16 expected_title =
        expect_success ? base::ASCIIToUTF16("link onload")
                       : base::ASCIIToUTF16("link onerror");
    content::TitleWatcher title_watcher(
        browser->tab_strip_model()->GetActiveWebContents(), expected_title);
    ui_test_utils::NavigateToURL(browser, url);
    return expected_title == title_watcher.WaitAndGetTitle();
  }

 private:
  bool disabled_via_field_trial_;
};

class PrefetchBrowserTestPrediction : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPrediction() : PrefetchBrowserTestBase(false) {}
};

class PrefetchBrowserTestPredictionDisabled : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionDisabled() : PrefetchBrowserTestBase(true) {}
};

// URLRequestJob (and associated handler) which hangs.
class HangingURLRequestJob : public net::URLRequestJob {
 public:
  HangingURLRequestJob(net::URLRequest* request,
                       net::NetworkDelegate* network_delegate)
      : net::URLRequestJob(request, network_delegate) {}

  // net::URLRequestJob implementation
  virtual void Start() OVERRIDE {}

 private:
  virtual ~HangingURLRequestJob() {}

  DISALLOW_COPY_AND_ASSIGN(HangingURLRequestJob);
};

class HangingRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit HangingRequestInterceptor(const base::Closure& callback)
      : callback_(callback) {}

  virtual ~HangingRequestInterceptor() {}

  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    if (!callback_.is_null())
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback_);
    return new HangingURLRequestJob(request, network_delegate);
  }

 private:
  base::Closure callback_;
};

void CreateHangingRequestInterceptorOnIO(const GURL& url,
                                         base::Closure callback) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<net::URLRequestInterceptor> never_respond_handler(
      new HangingRequestInterceptor(callback));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, never_respond_handler.Pass());
}

// Prefetch is disabled via field experiment.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionDisabled,
                       ExperimentDisabled) {
  CHECK(test_server()->Start());
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  // Should not prefetch even if preference is ALWAYS.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_ALWAYS);
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Prefetch should be allowed depending on preference and network type.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPrediction, PreferenceWorks) {
  CHECK(test_server()->Start());
  // Set real NetworkChangeNotifier singleton aside.
  scoped_ptr<NetworkChangeNotifier::DisableForTest> disable_for_test(
      new NetworkChangeNotifier::DisableForTest);

  // Preference defaults to WIFI_ONLY: prefetch when not on cellular.
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  }

  // Set preference to ALWAYS: always prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_ALWAYS);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }

  // Set preference to NEVER: never prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_NEVER);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  }
}

// Bug 339909: When in incognito mode the browser crashed due to an
// uninitialized preference member. Verify that it no longer does.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPrediction, IncognitoTest) {
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = new Browser(
      Browser::CreateParams(incognito_profile, browser()->host_desktop_type()));

  // Navigate just to have a tab in this window, otherwise there is no
  // WebContents for the incognito browser.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  CHECK(test_server()->Start());
  EXPECT_TRUE(RunPrefetchExperiment(true, incognito_browser));
}

// This test will verify the following:
// - that prefetches from the browser are actually launched
// - if a prefetch is in progress, but the originating renderer is destroyed,
//   that the pending prefetch request is cleaned up cleanly and does not
//   result in a crash.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPrediction, PrefetchFromBrowser) {
  const GURL kHangingUrl("http://hanging-url.com");
  base::RunLoop loop_;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&CreateHangingRequestInterceptorOnIO,
                                     kHangingUrl,
                                     loop_.QuitClosure()));
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  content::RenderFrameHost* rfh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  rfh->Send(new PrefetchMsg_Prefetch(rfh->GetRoutingID(), kHangingUrl));
  loop_.Run();
}

}  // namespace
