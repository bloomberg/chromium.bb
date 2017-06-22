// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/link_header_support.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kMockProviderId = 1;

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

}  // namespace

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

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }
  ServiceWorkerProviderHost* provider_host() { return provider_host_.get(); }
  int render_process_id() const { return helper_->mock_render_process_id(); }

  void CreateDocumentProviderHost() {
    // An empty host.
    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(render_process_id(), kMockProviderId,
                                    true /* is_parent_frame_secure */,
                                    context()->AsWeakPtr(),
                                    &remote_endpoints_.back());
    provider_host_ = host->AsWeakPtr();
    EXPECT_FALSE(
        context()->GetProviderHost(host->process_id(), host->provider_id()));
    context()->AddProviderHost(std::move(host));
  }

  void CreateInsecureDocumentProviderHost() {
    // An empty host.
    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(render_process_id(), kMockProviderId,
                                    false /* is_parent_frame_secure */,
                                    context()->AsWeakPtr(),
                                    &remote_endpoints_.back());
    provider_host_ = host->AsWeakPtr();
    EXPECT_FALSE(
        context()->GetProviderHost(host->process_id(), host->provider_id()));
    context()->AddProviderHost(std::move(host));
  }

  void CreateServiceWorkerProviderHost() {
    scoped_refptr<ServiceWorkerRegistration> registration =
        new ServiceWorkerRegistration(GURL("https://host/scope"), 1L,
                                      context()->AsWeakPtr());
    scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
        registration.get(), GURL("https://host/script.js"), 1L,
        context()->AsWeakPtr());

    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForServiceWorkerContext(
            render_process_id(), true /* is_parent_frame_secure */,
            version.get(), context()->AsWeakPtr(), &remote_endpoints_.back());
    provider_host_ = host->AsWeakPtr();
    EXPECT_FALSE(
        context()->GetProviderHost(host->process_id(), host->provider_id()));
    context()->AddProviderHost(std::move(host));
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& request_url,
                                                 ResourceType resource_type,
                                                 int provider_id) {
    std::unique_ptr<net::URLRequest> request = request_context_.CreateRequest(
        request_url, net::DEFAULT_PRIORITY, &request_delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    ResourceRequestInfo::AllocateForTesting(
        request.get(), resource_type, &resource_context_,
        -1 /* render_process_id */, -1 /* render_view_id */,
        -1 /* render_frame_id */, resource_type == RESOURCE_TYPE_MAIN_FRAME,
        false /* parent_is_main_frame */, true /* allow_download */,
        true /* is_async */, PREVIEWS_OFF /* previews_state */);
    ResourceRequestInfoImpl::ForRequest(request.get())
        ->set_initiated_in_secure_context_for_testing(true);

    ServiceWorkerRequestHandler::InitializeHandler(
        request.get(), context_wrapper(), &blob_storage_context_,
        render_process_id(), provider_id, false /* skip_service_worker */,
        FETCH_REQUEST_MODE_NO_CORS, FETCH_CREDENTIALS_MODE_OMIT,
        FetchRedirectMode::FOLLOW_MODE, resource_type,
        REQUEST_CONTEXT_TYPE_HYPERLINK, REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
        nullptr);

    return request;
  }

  std::unique_ptr<net::URLRequest> CreateSubresourceRequest(
      const GURL& request_url,
      int provider_id) {
    return CreateRequest(request_url, RESOURCE_TYPE_SCRIPT, provider_id);
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

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  net::TestURLRequestContext request_context_;
  net::TestDelegate request_delegate_;
  MockResourceContext resource_context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  storage::BlobStorageContext blob_storage_context_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;
};

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_Basic) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foo/bar/"),
                               provider_host()->provider_id())
          .get(),
      "<../foo.js>; rel=serviceworker", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/foo.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeWithFragment) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foo/bar/"),
                               provider_host()->provider_id())
          .get(),
      "<../bar.js>; rel=serviceworker; scope=\"scope#ref\"", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/bar/scope"),
            registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeAbsoluteUrl) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foo/bar/"),
                               provider_host()->provider_id())
          .get(),
      "<bar.js>; rel=serviceworker; "
      "scope=\"https://example.com:443/foo/bar/scope\"",
      context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/bar/scope"),
            registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/bar/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeDifferentOrigin) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<bar.js>; rel=serviceworker; scope=\"https://google.com/scope\"",
      context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeUrlEncodedSlash) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<bar.js>; rel=serviceworker; scope=\"./foo%2Fbar\"", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ScriptUrlEncodedSlash) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<foo%2Fbar.js>; rel=serviceworker", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScriptAbsoluteUrl) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<https://example.com/bar.js>; rel=serviceworker; scope=foo",
      context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/foo"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ScriptDifferentOrigin) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<https://google.com/bar.js>; rel=serviceworker; scope=foo",
      context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_MultipleWorkers) {
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<bar.js>; rel=serviceworker; scope=foo, <baz.js>; "
      "rel=serviceworker; scope=scope",
      context_wrapper());
  base::RunLoop().RunUntilIdle();

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
  CreateDocumentProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foobar/"),
                               provider_host()->provider_id())
          .get(),
      "<https://google.com/bar.js>; rel=serviceworker; scope=foo, <baz.js>; "
      "rel=serviceworker; scope=scope",
      context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/scope"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/baz.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_InsecureContext) {
  CreateDocumentProviderHost();
  std::unique_ptr<net::URLRequest> request = CreateSubresourceRequest(
      GURL("https://example.com/foo/bar/"), provider_host()->provider_id());
  ResourceRequestInfoImpl::ForRequest(request.get())
      ->set_initiated_in_secure_context_for_testing(false);
  ProcessLinkHeaderForRequest(request.get(), "<../foo.js>; rel=serviceworker",
                              context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_NavigationFromInsecureContextToSecureContext) {
  CreateDocumentProviderHost();
  std::unique_ptr<net::URLRequest> request =
      CreateRequest(GURL("https://example.com/foo/bar/"),
                    RESOURCE_TYPE_MAIN_FRAME, provider_host()->provider_id());
  ResourceRequestInfoImpl::ForRequest(request.get())
      ->set_initiated_in_secure_context_for_testing(false);

  provider_host()->SetDocumentUrl(GURL("https://example.com/foo/bar/"));

  ProcessLinkHeaderForRequest(request.get(), "<../foo.js>; rel=serviceworker",
                              context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/foo.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_NavigationToInsecureContext) {
  CreateDocumentProviderHost();
  provider_host()->SetDocumentUrl(GURL("http://example.com/foo/bar/"));
  ProcessLinkHeaderForRequest(
      CreateRequest(GURL("http://example.com/foo/bar/"),
                    RESOURCE_TYPE_MAIN_FRAME, provider_host()->provider_id())
          .get(),
      "<../foo.js>; rel=serviceworker", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_NavigationToInsecureHttpsContext) {
  CreateInsecureDocumentProviderHost();
  provider_host()->SetDocumentUrl(GURL("https://example.com/foo/bar/"));
  ProcessLinkHeaderForRequest(
      CreateRequest(GURL("https://example.com/foo/bar/"),
                    RESOURCE_TYPE_MAIN_FRAME, provider_host()->provider_id())
          .get(),
      "<../foo.js>; rel=serviceworker", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_FromWorkerWithoutControllees) {
  CreateServiceWorkerProviderHost();
  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foo/bar/"),
                               provider_host()->provider_id())
          .get(),
      "<../foo.js>; rel=serviceworker", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_FromWorkerWithControllees) {
  CreateServiceWorkerProviderHost();

  remote_endpoints_.emplace_back();
  std::unique_ptr<ServiceWorkerProviderHost> controllee =
      CreateProviderHostForWindow(render_process_id(), kMockProviderId,
                                  true /* is_parent_frame_secure */,
                                  context()->AsWeakPtr(),
                                  &remote_endpoints_.back());
  provider_host()->running_hosted_version()->AddControllee(controllee.get());

  ProcessLinkHeaderForRequest(
      CreateSubresourceRequest(GURL("https://example.com/foo/bar/"),
                               provider_host()->provider_id())
          .get(),
      "<../foo.js>; rel=serviceworker", context_wrapper());
  base::RunLoop().RunUntilIdle();

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
}

}  // namespace content
