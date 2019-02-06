// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

class SignedExchangeLoaderTest : public testing::Test {
 public:
  SignedExchangeLoaderTest() = default;
  ~SignedExchangeLoaderTest() override = default;

 protected:
  class MockURLLoaderClient final : public network::mojom::URLLoaderClient {
   public:
    explicit MockURLLoaderClient(network::mojom::URLLoaderClientRequest request)
        : loader_client_binding_(this, std::move(request)) {}
    ~MockURLLoaderClient() override {}

    // network::mojom::URLLoaderClient overrides:
    MOCK_METHOD1(OnReceiveResponse, void(const network::ResourceResponseHead&));
    MOCK_METHOD2(OnReceiveRedirect,
                 void(const net::RedirectInfo&,
                      const network::ResourceResponseHead&));
    MOCK_METHOD3(OnUploadProgress,
                 void(int64_t, int64_t, base::OnceCallback<void()> callback));
    MOCK_METHOD1(OnReceiveCachedMetadata, void(const std::vector<uint8_t>&));
    MOCK_METHOD1(OnTransferSizeUpdated, void(int32_t));
    MOCK_METHOD1(OnStartLoadingResponseBody,
                 void(mojo::ScopedDataPipeConsumerHandle));
    MOCK_METHOD1(OnComplete, void(const network::URLLoaderCompletionStatus&));

   private:
    mojo::Binding<network::mojom::URLLoaderClient> loader_client_binding_;
    DISALLOW_COPY_AND_ASSIGN(MockURLLoaderClient);
  };

  class MockURLLoader final : public network::mojom::URLLoader {
   public:
    explicit MockURLLoader(network::mojom::URLLoaderRequest url_loader_request)
        : binding_(this, std::move(url_loader_request)) {}
    ~MockURLLoader() override = default;

    // network::mojom::URLLoader overrides:
    MOCK_METHOD3(FollowRedirect,
                 void(const std::vector<std::string>&,
                      const net::HttpRequestHeaders&,
                      const base::Optional<GURL>&));
    MOCK_METHOD0(ProceedWithResponse, void());
    MOCK_METHOD2(SetPriority,
                 void(net::RequestPriority priority,
                      int32_t intra_priority_value));
    MOCK_METHOD0(PauseReadingBodyFromNet, void());
    MOCK_METHOD0(ResumeReadingBodyFromNet, void());

   private:
    mojo::Binding<network::mojom::URLLoader> binding_;

    DISALLOW_COPY_AND_ASSIGN(MockURLLoader);
  };

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeLoaderTest);
};

TEST_F(SignedExchangeLoaderTest, Simple) {
  network::mojom::URLLoaderPtr loader;
  network::mojom::URLLoaderClientPtr loader_client;
  MockURLLoader mock_loader(mojo::MakeRequest(&loader));
  network::mojom::URLLoaderClientEndpointsPtr endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          std::move(loader).PassInterface(), mojo::MakeRequest(&loader_client));

  network::mojom::URLLoaderClientPtr client;
  MockURLLoaderClient mock_client(mojo::MakeRequest(&client));

  network::ResourceRequest resource_request;
  resource_request.url = GURL("https://example.com/test.sxg");

  network::ResourceResponseHead response;
  std::string headers("HTTP/1.1 200 OK\nnContent-type: foo/bar\n\n");
  response.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));

  MockSignedExchangeHandlerFactory factory(
      SignedExchangeLoadResult::kSuccess, net::OK,
      GURL("https://publisher.example.com/"), "text/html", {});

  SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(&factory);
  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader =
      std::make_unique<SignedExchangeLoader>(
          resource_request, response, std::move(client), std::move(endpoints),
          network::mojom::kURLLoadOptionNone,
          false /* should_redirect_to_fallback */, nullptr /* devtools_proxy */,
          nullptr /* url_loader_factory */,
          SignedExchangeLoader::URLLoaderThrottlesGetter(),
          base::RepeatingCallback<int(void)>(), nullptr /* metric_recorder */);

  EXPECT_CALL(mock_loader, PauseReadingBodyFromNet());
  signed_exchange_loader->PauseReadingBodyFromNet();

  EXPECT_CALL(mock_loader, ResumeReadingBodyFromNet());
  signed_exchange_loader->ResumeReadingBodyFromNet();

  constexpr int kIntraPriority = 5;
  EXPECT_CALL(mock_loader,
              SetPriority(net::RequestPriority::HIGHEST, kIntraPriority));
  signed_exchange_loader->SetPriority(net::RequestPriority::HIGHEST,
                                      kIntraPriority);

  EXPECT_CALL(mock_client, OnReceiveRedirect(_, _));
  base::RunLoop().RunUntilIdle();

  const std::string kTestString = "Hello, world!";
  mojo::DataPipe data_pipe(static_cast<uint32_t>(kTestString.size()));
  std::unique_ptr<mojo::StringDataPipeProducer> producer =
      std::make_unique<mojo::StringDataPipeProducer>(
          std::move(data_pipe.producer_handle));

  mojo::StringDataPipeProducer* raw_producer = producer.get();
  raw_producer->Write(
      kTestString,
      mojo::StringDataPipeProducer::AsyncWritingMode::
          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION,
      base::BindOnce([](std::unique_ptr<mojo::StringDataPipeProducer> producer,
                        MojoResult result) {},
                     std::move(producer)));

  loader_client->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  network::URLLoaderCompletionStatus status;
  loader_client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  base::RunLoop().RunUntilIdle();

  network::mojom::URLLoaderClientPtr client_after_redirect;
  MockURLLoaderClient mock_client_after_redirect(
      mojo::MakeRequest(&client_after_redirect));
  EXPECT_CALL(mock_client_after_redirect, OnReceiveResponse(_));
  EXPECT_CALL(mock_client_after_redirect, OnStartLoadingResponseBody(_));
  EXPECT_CALL(mock_client_after_redirect, OnComplete(_));

  signed_exchange_loader->ConnectToClient(std::move(client_after_redirect));
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
