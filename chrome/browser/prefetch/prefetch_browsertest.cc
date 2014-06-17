// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
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
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"

using content::BrowserThread;

namespace {

const char kPrefetchPage[] = "files/prerender/simple_prefetch.html";

class PrefetchBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit PrefetchBrowserTestBase(bool do_predictive_networking,
                                   bool do_prefetch_field_trial)
      : do_predictive_networking_(do_predictive_networking),
        do_prefetch_field_trial_(do_prefetch_field_trial) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (do_prefetch_field_trial_) {
      command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                      "Prefetch/ExperimentYes/");
    } else {
      command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                      "Prefetch/ExperimentNo/");
    }
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionEnabled, do_predictive_networking_);
  }

  bool RunPrefetchExperiment(bool expect_success, Browser* browser) {
    CHECK(test_server()->Start());
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
  bool do_predictive_networking_;
  bool do_prefetch_field_trial_;
};

class PrefetchBrowserTestPredictionOnExpOn : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOnExpOn()
      : PrefetchBrowserTestBase(true, true) {}
};

class PrefetchBrowserTestPredictionOnExpOff : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOnExpOff()
      : PrefetchBrowserTestBase(true, false) {}
};

class PrefetchBrowserTestPredictionOffExpOn : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOffExpOn()
      : PrefetchBrowserTestBase(false, true) {}
};

class PrefetchBrowserTestPredictionOffExpOff : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOffExpOff()
      : PrefetchBrowserTestBase(false, false) {}
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

// Privacy option is on, experiment is on.  Prefetch should succeed.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOn, PredOnExpOn) {
  EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
}

// Privacy option is on, experiment is off.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOff, PredOnExpOff) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Privacy option is off, experiment is on.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOffExpOn, PredOffExpOn) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Privacy option is off, experiment is off.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOffExpOff, PredOffExpOff) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Bug 339909: When in incognito mode the browser crashed due to an
// uninitialized preference member. Verify that it no longer does.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOn, IncognitoTest) {
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = new Browser(
      Browser::CreateParams(incognito_profile, browser()->host_desktop_type()));

  // Navigate just to have a tab in this window, otherwise there is no
  // WebContents for the incognito browser.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(RunPrefetchExperiment(true, incognito_browser));
}

// This test will verify the following:
// - that prefetches from the browser are actually launched
// - if a prefetch is in progress, but the originating renderer is destroyed,
//   that the pending prefetch request is cleaned up cleanly and does not
//   result in a crash.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOn,
                       PrefetchFromBrowser) {
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
