// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/network/network_context.h"
#include "content/network/network_service_impl.h"
#include "content/public/common/network_service.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace content {

namespace {

class NetworkContextTest : public testing::Test {
 public:
  NetworkContextTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        network_service_(NetworkServiceImpl::CreateForTesting()) {}
  ~NetworkContextTest() override {}

  std::unique_ptr<NetworkContext> CreateContextWithParams(
      mojom::NetworkContextParamsPtr context_params) {
    return base::MakeUnique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
        std::move(context_params));
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkServiceImpl> network_service_;
  // Stores the NetworkContextPtr of the most recently created NetworkContext,
  // since destroying this before the NetworkContext itself triggers deletion of
  // the NetworkContext. These tests are probably fine anyways, since the
  // message loop must be spun for that to happen.
  mojom::NetworkContextPtr network_context_ptr_;
};

TEST_F(NetworkContextTest, DisableDataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->enable_data_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, EnableDataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->enable_data_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, DisableFileUrlSupport) {
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->enable_file_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
TEST_F(NetworkContextTest, EnableFileUrlSupport) {
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->enable_file_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}
#endif  // !BUILDFLAG(DISABLE_FILE_SUPPORT)

TEST_F(NetworkContextTest, DisableFtpUrlSupport) {
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->enable_ftp_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST_F(NetworkContextTest, EnableFtpUrlSupport) {
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->enable_ftp_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

}  // namespace

}  // namespace content
