// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

using content::InterstitialPage;
using content::WebContents;
using extensions::ExperienceSamplingEvent;

typedef net::BaseTestServer::SSLOptions SSLOptions;

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

class ExperienceSamplingApiTest : public ExtensionApiTest {
 public:
  ExperienceSamplingApiTest() {}
  virtual ~ExperienceSamplingApiTest() {}

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    InProcessBrowserTest::SetUpOnMainThread();
    extension_ =
        LoadExtension(test_data_dir_.AppendASCII("experience_sampling_private")
                          .AppendASCII("events"));
    listener_.reset(new ExtensionTestMessageListener(false));
  }

  const extensions::Extension* extension_;
  scoped_ptr<ExtensionTestMessageListener> listener_;
};

class ExperienceSamplingSSLInterstitialTest : public ExperienceSamplingApiTest {
 public:
  ExperienceSamplingSSLInterstitialTest()
      : https_server_mismatched_(net::SpawnedTestServer::TYPE_HTTPS,
                                 SSLOptions(SSLOptions::CERT_MISMATCHED_NAME),
                                 base::FilePath(kDocRoot)) {}

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    ExperienceSamplingApiTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server_mismatched_.Start());
    ui_test_utils::NavigateToURL(
        browser(), https_server_mismatched_.GetURL("files/ssl/google.html"));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(tab);
    interstitial_page_ = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page_);
  }

  InterstitialPage* interstitial_page_;
  net::SpawnedTestServer https_server_mismatched_;
};

IN_PROC_BROWSER_TEST_F(ExperienceSamplingSSLInterstitialTest, ProceedEvent) {
  // Trigger an OnDecision.
  interstitial_page_->Proceed();

  // Get the message and make sure it matches an ssl_interstitial and kProceed
  // event. A match is when find() != npos.
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  ASSERT_NE(listener_->message().find("ssl_interstitial"), std::string::npos);
  ASSERT_NE(listener_->message().find(ExperienceSamplingEvent::kProceed),
            std::string::npos);
}

IN_PROC_BROWSER_TEST_F(ExperienceSamplingSSLInterstitialTest, DenyEvent) {
  // Trigger an OnDecision.
  interstitial_page_->DontProceed();

  // Get the message and make sure it matches an ssl_interstitial and kDeny
  // event. A match is when find() != npos.
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  ASSERT_NE(listener_->message().find("ssl_interstitial"), std::string::npos);
  ASSERT_NE(listener_->message().find(ExperienceSamplingEvent::kDeny),
            std::string::npos);
}

IN_PROC_BROWSER_TEST_F(ExperienceSamplingSSLInterstitialTest,
                       NoEventsFromIncognito) {
  // Loads an SSL interstitial in an incognito window while listening for an
  // OnDecision. The extension shouldn't hear the event.
  Browser* incognito_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(),
      https_server_mismatched_.GetURL("files/ssl/google.html"));
  ASSERT_TRUE(incognito_browser->profile()->IsOffTheRecord());
  WebContents* tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  ASSERT_TRUE(tab->GetBrowserContext()->IsOffTheRecord());

  InterstitialPage* incognito_interstitial = tab->GetInterstitialPage();
  ASSERT_TRUE(incognito_interstitial);
  incognito_interstitial->DontProceed();

  // We should never get the message (the extension will timeout and send us a
  // "timeout" message instead of the API event message).
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  ASSERT_EQ(listener_->message(), "timeout") << "Got a non-timeout message";
}

}  // namespace
