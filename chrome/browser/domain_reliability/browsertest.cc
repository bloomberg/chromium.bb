// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/domain_reliability/service.h"
#include "net/base/net_errors.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "url/gurl.h"

namespace domain_reliability {

class DomainReliabilityBrowserTest : public InProcessBrowserTest {
 public:
  DomainReliabilityBrowserTest() {
    net::URLRequestFailedJob::AddUrlHandler();
  }

  ~DomainReliabilityBrowserTest() override {}

  // Note: In an ideal world, instead of appending the command-line switch and
  // manually setting discard_uploads to false, Domain Reliability would
  // continuously monitor the metrics reporting pref, and the test could just
  // set the pref.

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomainReliability);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    DomainReliabilityService* service = GetService();
    if (service)
      service->SetDiscardUploadsForTesting(false);
  }

  DomainReliabilityService* GetService() {
    return DomainReliabilityServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityBrowserTest);
};

class DomainReliabilityDisabledBrowserTest
    : public DomainReliabilityBrowserTest {
 protected:
  DomainReliabilityDisabledBrowserTest() {}

  ~DomainReliabilityDisabledBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableDomainReliability);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityDisabledBrowserTest);
};

IN_PROC_BROWSER_TEST_F(DomainReliabilityDisabledBrowserTest,
                       ServiceNotCreated) {
  EXPECT_FALSE(GetService());
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityBrowserTest, ServiceCreated) {
  EXPECT_TRUE(GetService());
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityBrowserTest, UploadAtShutdown) {
  DomainReliabilityService* service = GetService();

  auto config = base::MakeUnique<DomainReliabilityConfig>();
  config->origin = GURL("https://localhost/");
  config->include_subdomains = false;
  config->collectors.push_back(base::MakeUnique<GURL>(
      net::URLRequestFailedJob::GetMockHttpsUrl(net::ERR_IO_PENDING)));
  config->success_sample_rate = 1.0;
  config->failure_sample_rate = 1.0;
  service->AddContextForTesting(std::move(config));

  ui_test_utils::NavigateToURL(browser(), GURL("https://localhost/"));

  service->ForceUploadsForTesting();

  // At this point, there is an upload pending. If everything goes well, the
  // test will finish, destroy the profile, and Domain Reliability will shut
  // down properly. If things go awry, it may crash as terminating the pending
  // upload calls into already-destroyed parts of the component.
}

}  // namespace domain_reliability
