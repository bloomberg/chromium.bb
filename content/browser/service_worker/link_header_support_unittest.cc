// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/link_header_support.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SaveFoundRegistrationsCallback(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    std::vector<ServiceWorkerRegistrationInfo>* registrations,
    ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& result) {
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registrations = result;
}

ServiceWorkerContextWrapper::GetRegistrationsInfosCallback
SaveFoundRegistrations(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    std::vector<ServiceWorkerRegistrationInfo>* registrations) {
  *called = false;
  return base::Bind(&SaveFoundRegistrationsCallback, expected_status, called,
                    registrations);
}

class LinkHeaderServiceWorkerTest : public ::testing::Test {
 public:
  LinkHeaderServiceWorkerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        resource_context_(&request_context_) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
  ~LinkHeaderServiceWorkerTest() override {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void ProcessLinkHeader(const GURL& request_url,
                         const std::string& link_header) {
    std::unique_ptr<net::URLRequest> request = request_context_.CreateRequest(
        request_url, net::DEFAULT_PRIORITY, &request_delegate_);
    ResourceRequestInfo::AllocateForTesting(
        request.get(), RESOURCE_TYPE_SCRIPT, &resource_context_,
        -1 /* render_process_id */, -1 /* render_view_id */,
        -1 /* render_frame_id */, false /* is_main_frame */,
        false /* parent_is_main_frame */, true /* allow_download */,
        true /* is_async */, false /* is_using_lofi */);

    ProcessLinkHeaderForRequest(request.get(), link_header, context_wrapper());
    base::RunLoop().RunUntilIdle();
  }

  std::vector<ServiceWorkerRegistrationInfo> GetRegistrations() {
    bool called;
    std::vector<ServiceWorkerRegistrationInfo> registrations;
    context_wrapper()->GetAllRegistrations(
        SaveFoundRegistrations(SERVICE_WORKER_OK, &called, &registrations));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
    return registrations;
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  net::TestURLRequestContext request_context_;
  net::TestDelegate request_delegate_;
  MockResourceContext resource_context_;
};

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_Basic) {
  ProcessLinkHeader(GURL("https://example.com/foo/bar/"),
                    "<../foo.js>; rel=serviceworker");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/foo.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeWithFragment) {
  ProcessLinkHeader(GURL("https://example.com/foo/bar/"),
                    "<../bar.js>; rel=serviceworker; scope=\"scope#ref\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/bar/scope"),
            registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeAbsoluteUrl) {
  ProcessLinkHeader(GURL("https://example.com/foo/bar/"),
                    "<bar.js>; rel=serviceworker; "
                    "scope=\"https://example.com:443/foo/bar/scope\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/bar/scope"),
            registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/bar/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeDifferentOrigin) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<bar.js>; rel=serviceworker; scope=\"https://google.com/scope\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeUrlEncodedSlash) {
  ProcessLinkHeader(GURL("https://example.com/foobar/"),
                    "<bar.js>; rel=serviceworker; scope=\"./foo%2Fbar\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ScriptUrlEncodedSlash) {
  ProcessLinkHeader(GURL("https://example.com/foobar/"),
                    "<foo%2Fbar.js>; rel=serviceworker");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScriptAbsoluteUrl) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<https://example.com/bar.js>; rel=serviceworker; scope=foo");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/foo"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ScriptDifferentOrigin) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<https://google.com/bar.js>; rel=serviceworker; scope=foo");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_MultipleWorkers) {
  ProcessLinkHeader(GURL("https://example.com/foobar/"),
                    "<bar.js>; rel=serviceworker; scope=foo, <baz.js>; "
                    "rel=serviceworker; scope=scope");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(2u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/foo"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/bar.js"),
            registrations[0].active_version.script_url);
  EXPECT_EQ(GURL("https://example.com/foobar/scope"), registrations[1].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/baz.js"),
            registrations[1].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ValidAndInvalidValues) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<https://google.com/bar.js>; rel=serviceworker; scope=foo, <baz.js>; "
      "rel=serviceworker; scope=scope");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/scope"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/baz.js"),
            registrations[0].active_version.script_url);
}

}  // namespace

}  // namespace content
