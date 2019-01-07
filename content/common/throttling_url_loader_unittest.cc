// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/throttling_url_loader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/url_loader_throttle.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

GURL request_url = GURL("http://example.org");
GURL redirect_url = GURL("http://example.com");

class TestURLLoaderFactory : public network::mojom::URLLoaderFactory,
                             public network::mojom::URLLoader {
 public:
  TestURLLoaderFactory() : binding_(this), url_loader_binding_(this) {
    binding_.Bind(mojo::MakeRequest(&factory_ptr_));
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            factory_ptr_.get());
  }

  ~TestURLLoaderFactory() override { shared_factory_->Detach(); }

  network::mojom::URLLoaderFactoryPtr& factory_ptr() { return factory_ptr_; }
  network::mojom::URLLoaderClientPtr& client_ptr() { return client_ptr_; }
  mojo::Binding<network::mojom::URLLoader>& url_loader_binding() {
    return url_loader_binding_;
  }
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory() {
    return shared_factory_;
  }

  size_t create_loader_and_start_called() const {
    return create_loader_and_start_called_;
  }

  const std::vector<std::string>& headers_removed_on_redirect() const {
    return headers_removed_on_redirect_;
  }

  const net::HttpRequestHeaders& headers_modified_on_redirect() const {
    return headers_modified_on_redirect_;
  }

  size_t pause_reading_body_from_net_called() const {
    return pause_reading_body_from_net_called_;
  }

  size_t resume_reading_body_from_net_called() const {
    return resume_reading_body_from_net_called_;
  }

  void NotifyClientOnReceiveResponse() {
    client_ptr_->OnReceiveResponse(network::ResourceResponseHead());
  }

  void NotifyClientOnReceiveRedirect() {
    net::RedirectInfo info;
    info.new_url = redirect_url;
    client_ptr_->OnReceiveRedirect(info, network::ResourceResponseHead());
  }

  void NotifyClientOnComplete(int error_code) {
    network::URLLoaderCompletionStatus data;
    data.error_code = error_code;
    client_ptr_->OnComplete(data);
  }

  void CloseClientPipe() { client_ptr_.reset(); }

  using OnCreateLoaderAndStartCallback = base::RepeatingCallback<void(
      const network::ResourceRequest& url_request)>;
  void set_on_create_loader_and_start(
      const OnCreateLoaderAndStartCallback& callback) {
    on_create_loader_and_start_callback_ = callback;
  }

 private:
  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    create_loader_and_start_called_++;

    if (url_loader_binding_.is_bound())
      url_loader_binding_.Unbind();

    url_loader_binding_.Bind(std::move(request));
    client_ptr_ = std::move(client);

    if (on_create_loader_and_start_callback_)
      on_create_loader_and_start_callback_.Run(url_request);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    NOTREACHED();
  }

  // network::mojom::URLLoader implementation.
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {
    headers_removed_on_redirect_ = removed_headers;
    headers_modified_on_redirect_ = modified_headers;
  }

  void ProceedWithResponse() override {}

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

  void PauseReadingBodyFromNet() override {
    pause_reading_body_from_net_called_++;
  }

  void ResumeReadingBodyFromNet() override {
    resume_reading_body_from_net_called_++;
  }

  size_t create_loader_and_start_called_ = 0;
  std::vector<std::string> headers_removed_on_redirect_;
  net::HttpRequestHeaders headers_modified_on_redirect_;
  size_t pause_reading_body_from_net_called_ = 0;
  size_t resume_reading_body_from_net_called_ = 0;

  mojo::Binding<network::mojom::URLLoaderFactory> binding_;
  mojo::Binding<network::mojom::URLLoader> url_loader_binding_;
  network::mojom::URLLoaderFactoryPtr factory_ptr_;
  network::mojom::URLLoaderClientPtr client_ptr_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory> shared_factory_;
  OnCreateLoaderAndStartCallback on_create_loader_and_start_callback_;
  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class TestURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  TestURLLoaderClient() {}

  size_t on_received_response_called() const {
    return on_received_response_called_;
  }

  size_t on_received_redirect_called() const {
    return on_received_redirect_called_;
  }

  size_t on_complete_called() const { return on_complete_called_; }

  void set_on_received_redirect_callback(
      const base::RepeatingClosure& callback) {
    on_received_redirect_callback_ = callback;
  }

  void set_on_received_response_callback(
      const base::RepeatingClosure& callback) {
    on_received_response_callback_ = callback;
  }

  using OnCompleteCallback = base::Callback<void(int error_code)>;
  void set_on_complete_callback(const OnCompleteCallback& callback) {
    on_complete_callback_ = callback;
  }

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override {
    on_received_response_called_++;
    if (on_received_response_callback_)
      on_received_response_callback_.Run();
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override {
    on_received_redirect_called_++;
    if (on_received_redirect_callback_)
      on_received_redirect_callback_.Run();
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    on_complete_called_++;
    if (on_complete_callback_)
      on_complete_callback_.Run(status.error_code);
  }

  size_t on_received_response_called_ = 0;
  size_t on_received_redirect_called_ = 0;
  size_t on_complete_called_ = 0;

  base::RepeatingClosure on_received_redirect_callback_;
  base::RepeatingClosure on_received_response_callback_;
  OnCompleteCallback on_complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderClient);
};

class TestURLLoaderThrottle : public URLLoaderThrottle {
 public:
  TestURLLoaderThrottle() {}
  explicit TestURLLoaderThrottle(const base::Closure& destruction_notifier)
      : destruction_notifier_(destruction_notifier) {}

  ~TestURLLoaderThrottle() override {
    if (destruction_notifier_)
      destruction_notifier_.Run();
  }

  using ThrottleCallback =
      base::RepeatingCallback<void(URLLoaderThrottle::Delegate* delegate,
                                   bool* defer)>;
  using ThrottleRedirectCallback =
      base::RepeatingCallback<void(URLLoaderThrottle::Delegate* delegate,
                                   bool* defer,
                                   std::vector<std::string>* removed_headers,
                                   net::HttpRequestHeaders* modified_headers)>;

  size_t will_start_request_called() const {
    return will_start_request_called_;
  }
  size_t will_redirect_request_called() const {
    return will_redirect_request_called_;
  }
  size_t will_process_response_called() const {
    return will_process_response_called_;
  }
  size_t before_will_process_response_called() const {
    return before_will_process_response_called_;
  }

  GURL observed_response_url() const { return response_url_; }

  void set_will_start_request_callback(const ThrottleCallback& callback) {
    will_start_request_callback_ = callback;
  }

  void set_will_redirect_request_callback(
      const ThrottleRedirectCallback& callback) {
    will_redirect_request_callback_ = callback;
  }

  void set_will_process_response_callback(const ThrottleCallback& callback) {
    will_process_response_callback_ = callback;
  }

  void set_before_will_process_response_callback(
      const ThrottleCallback& callback) {
    before_will_process_response_callback_ = callback;
  }

  void set_modify_url_in_will_start(const GURL& url) {
    modify_url_in_will_start_ = url;
  }

  Delegate* delegate() const { return delegate_; }

 private:
  // URLLoaderThrottle implementation.
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    will_start_request_called_++;
    if (!modify_url_in_will_start_.is_empty())
      request->url = modify_url_in_will_start_;

    if (will_start_request_callback_)
      will_start_request_callback_.Run(delegate_, defer);
  }

  void WillRedirectRequest(net::RedirectInfo* redirect_info,
                           const network::ResourceResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* removed_headers,
                           net::HttpRequestHeaders* modified_headers) override {
    will_redirect_request_called_++;
    if (will_redirect_request_callback_) {
      will_redirect_request_callback_.Run(delegate_, defer, removed_headers,
                                          modified_headers);
    }
  }

  void WillProcessResponse(const GURL& response_url,
                           network::ResourceResponseHead* response_head,
                           bool* defer) override {
    will_process_response_called_++;
    if (will_process_response_callback_)
      will_process_response_callback_.Run(delegate_, defer);
    response_url_ = response_url;
  }

  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::ResourceResponseHead& response_head,
      bool* defer) override {
    before_will_process_response_called_++;
    if (before_will_process_response_callback_)
      before_will_process_response_callback_.Run(delegate_, defer);
  }

  size_t will_start_request_called_ = 0;
  size_t will_redirect_request_called_ = 0;
  size_t will_process_response_called_ = 0;
  size_t before_will_process_response_called_ = 0;

  GURL response_url_;

  ThrottleCallback will_start_request_callback_;
  ThrottleRedirectCallback will_redirect_request_callback_;
  ThrottleCallback will_process_response_callback_;
  ThrottleCallback before_will_process_response_callback_;

  GURL modify_url_in_will_start_;

  base::Closure destruction_notifier_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderThrottle);
};

class ThrottlingURLLoaderTest : public testing::Test {
 public:
  ThrottlingURLLoaderTest() : weak_factory_(this) {}

  std::unique_ptr<ThrottlingURLLoader>& loader() { return loader_; }
  TestURLLoaderThrottle* throttle() const { return throttle_; }

 protected:
  // testing::Test implementation.
  void SetUp() override {
    auto throttle = std::make_unique<TestURLLoaderThrottle>(
        base::Bind(&ThrottlingURLLoaderTest::ResetThrottleRawPointer,
                   weak_factory_.GetWeakPtr()));

    throttle_ = throttle.get();

    throttles_.push_back(std::move(throttle));
  }

  void CreateLoaderAndStart(bool sync = false) {
    uint32_t options = 0;
    if (sync)
      options |= network::mojom::kURLLoadOptionSynchronous;
    network::ResourceRequest request;
    request.url = request_url;
    loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        factory_.shared_factory(), std::move(throttles_), 0, 0, options,
        &request, &client_, TRAFFIC_ANNOTATION_FOR_TESTS,
        base::ThreadTaskRunnerHandle::Get());
    factory_.factory_ptr().FlushForTesting();
  }

  void ResetThrottleRawPointer() { throttle_ = nullptr; }

  // Be the first member so it is destroyed last.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<ThrottlingURLLoader> loader_;
  std::vector<std::unique_ptr<URLLoaderThrottle>> throttles_;

  TestURLLoaderFactory factory_;
  TestURLLoaderClient client_;

  // Owned by |throttles_| or |loader_|.
  TestURLLoaderThrottle* throttle_ = nullptr;

  base::WeakPtrFactory<ThrottlingURLLoaderTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingURLLoaderTest);
};

TEST_F(ThrottlingURLLoaderTest, CancelBeforeStart) {
  throttle_->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  CreateLoaderAndStart();
  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, DeferBeforeStart) {
  throttle_->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::OK, error);
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();
  factory_.factory_ptr().FlushForTesting();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ModifyURLBeforeStart) {
  throttle_->set_modify_url_in_will_start(GURL("http://example.org/foo"));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
}

TEST_F(ThrottlingURLLoaderTest, CancelBeforeRedirect) {
  throttle_->set_will_redirect_request_callback(base::BindRepeating(
      [](URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* /* removed_headers */,
         net::HttpRequestHeaders* /* modified_headers */) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveRedirect();

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, DeferBeforeRedirect) {
  base::RunLoop run_loop1;
  throttle_->set_will_redirect_request_callback(base::Bind(
      [](const base::Closure& quit_closure,
         URLLoaderThrottle::Delegate* delegate, bool* defer,
         std::vector<std::string>* /* removed_headers */,
         net::HttpRequestHeaders* /* modified_headers */) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_UNEXPECTED, error);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveRedirect();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  factory_.NotifyClientOnComplete(net::ERR_UNEXPECTED);

  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();
  run_loop2.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(1u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ModifyHeadersBeforeRedirect) {
  throttle_->set_will_redirect_request_callback(base::BindRepeating(
      [](URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers) {
        removed_headers->push_back("X-Test-Header-1");
        modified_headers->SetHeader("X-Test-Header-2", "Foo");
        modified_headers->SetHeader("X-Test-Header-3", "Throttle Value");
      }));

  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader("X-Test-Header-3", "Client Value");
    modified_headers.SetHeader("X-Test-Header-4", "Bar");
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers));
  }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(factory_.headers_removed_on_redirect().empty());
  EXPECT_THAT(factory_.headers_removed_on_redirect(),
              testing::ElementsAre("X-Test-Header-1"));
  ASSERT_FALSE(factory_.headers_modified_on_redirect().IsEmpty());
  EXPECT_EQ(
      "X-Test-Header-2: Foo\r\n"
      "X-Test-Header-3: Client Value\r\n"
      "X-Test-Header-4: Bar\r\n\r\n",
      factory_.headers_modified_on_redirect().ToString());
}

TEST_F(ThrottlingURLLoaderTest, MultipleThrottlesModifyHeadersBeforeRedirect) {
  auto* throttle2 = new TestURLLoaderThrottle();
  throttles_.push_back(base::WrapUnique(throttle2));

  throttle_->set_will_redirect_request_callback(base::BindRepeating(
      [](URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers) {
        removed_headers->push_back("X-Test-Header-0");
        removed_headers->push_back("X-Test-Header-1");
        modified_headers->SetHeader("X-Test-Header-3", "Foo");
        modified_headers->SetHeader("X-Test-Header-4", "Throttle1");
      }));

  throttle2->set_will_redirect_request_callback(base::BindRepeating(
      [](URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers) {
        removed_headers->push_back("X-Test-Header-1");
        removed_headers->push_back("X-Test-Header-2");
        modified_headers->SetHeader("X-Test-Header-4", "Throttle2");
      }));

  client_.set_on_received_redirect_callback(
      base::BindLambdaForTesting([&]() { loader_->FollowRedirect({}, {}); }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(factory_.headers_removed_on_redirect().empty());
  EXPECT_THAT(factory_.headers_removed_on_redirect(),
              testing::ElementsAre("X-Test-Header-0", "X-Test-Header-1",
                                   "X-Test-Header-2"));
  ASSERT_FALSE(factory_.headers_modified_on_redirect().IsEmpty());
  EXPECT_EQ(
      "X-Test-Header-3: Foo\r\n"
      "X-Test-Header-4: Throttle2\r\n\r\n",
      factory_.headers_modified_on_redirect().ToString());
}

TEST_F(ThrottlingURLLoaderTest, CancelBeforeResponse) {
  throttle_->set_will_process_response_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, DeferBeforeResponse) {
  base::RunLoop run_loop1;
  throttle_->set_will_process_response_callback(base::Bind(
      [](const base::Closure& quit_closure,
         URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_UNEXPECTED, error);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  factory_.NotifyClientOnComplete(net::ERR_UNEXPECTED);

  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();
  run_loop2.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, PipeClosure) {
  base::RunLoop run_loop;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_ABORTED, error);
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  CreateLoaderAndStart();

  factory_.CloseClientPipe();

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ResumeNoOpIfNotDeferred) {
  auto resume_callback = base::BindRepeating(
      [](URLLoaderThrottle::Delegate* delegate, bool* /* defer */) {
        delegate->Resume();
        delegate->Resume();
      });
  throttle_->set_will_start_request_callback(resume_callback);
  throttle_->set_will_process_response_callback(std::move(resume_callback));
  throttle_->set_will_redirect_request_callback(base::BindRepeating(
      [](URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* /* removed_headers */,
         net::HttpRequestHeaders* /* modified_headers */) {
        delegate->Resume();
        delegate->Resume();
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(redirect_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(1u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, CancelNoOpIfAlreadyCanceled) {
  throttle_->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
        delegate->CancelWithError(net::ERR_UNEXPECTED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  CreateLoaderAndStart();
  throttle_->delegate()->CancelWithError(net::ERR_INVALID_ARGUMENT);
  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ResumeNoOpIfAlreadyCanceled) {
  throttle_->set_will_process_response_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
        delegate->Resume();
      }));

  base::RunLoop run_loop1;
  client_.set_on_complete_callback(base::Bind(
      [](const base::Closure& quit_closure, int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop1.Run();

  throttle_->delegate()->Resume();

  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, MultipleThrottlesBasicSupport) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());
  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveResponse();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
}

TEST_F(ThrottlingURLLoaderTest, BlockWithOneOfMultipleThrottles) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());
  throttle2->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));

  base::RunLoop loop;
  client_.set_on_complete_callback(base::Bind(
      [](base::RunLoop* loop, int error) {
        EXPECT_EQ(net::OK, error);
        loop->Quit();
      },
      &loop));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle2->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(0u, throttle2->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle2->delegate()->Resume();
  factory_.factory_ptr().FlushForTesting();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle2->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(1u, throttle2->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));
  EXPECT_TRUE(
      throttle2->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, BlockWithMultipleThrottles) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());

  // Defers a request on both throttles.
  throttle_->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));
  throttle2->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));

  base::RunLoop loop;
  client_.set_on_complete_callback(base::Bind(
      [](base::RunLoop* loop, int error) {
        EXPECT_EQ(net::OK, error);
        loop->Quit();
      },
      &loop));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle2->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(0u, throttle2->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();

  // Should still not have started because there's |throttle2| is still blocking
  // the request.
  factory_.factory_ptr().FlushForTesting();
  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  throttle2->delegate()->Resume();

  // Now it should have started.
  factory_.factory_ptr().FlushForTesting();
  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle2->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(1u, throttle2->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));
  EXPECT_TRUE(
      throttle2->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, PauseResumeReadingBodyFromNet) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());

  // Test that it is okay to call delegate->PauseReadingBodyFromNet() even
  // before the loader is created.
  throttle_->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->PauseReadingBodyFromNet();
        *defer = true;
      }));
  throttle2->set_will_start_request_callback(
      base::Bind([](URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->PauseReadingBodyFromNet();
      }));

  CreateLoaderAndStart();

  throttle_->delegate()->Resume();

  factory_.factory_ptr().FlushForTesting();
  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  // Make sure all URLLoader calls before this point are delivered to the impl
  // side.
  factory_.url_loader_binding().FlushForTesting();

  // Although there were two calls to delegate->PauseReadingBodyFromNet(), only
  // one URLLoader::PauseReadingBodyFromNet() Mojo call was made.
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(0u, factory_.resume_reading_body_from_net_called());

  // Reading body from network is still paused by |throttle2|. Calling
  // ResumeReadingBodyFromNet() on |throttle_| shouldn't have any effect.
  throttle_->delegate()->ResumeReadingBodyFromNet();
  factory_.url_loader_binding().FlushForTesting();
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(0u, factory_.resume_reading_body_from_net_called());

  // Even if we call ResumeReadingBodyFromNet() on |throttle_| one more time.
  throttle_->delegate()->ResumeReadingBodyFromNet();
  factory_.url_loader_binding().FlushForTesting();
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(0u, factory_.resume_reading_body_from_net_called());

  throttle2->delegate()->ResumeReadingBodyFromNet();
  factory_.url_loader_binding().FlushForTesting();
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(1u, factory_.resume_reading_body_from_net_called());
}

TEST_F(ThrottlingURLLoaderTest,
       DestroyingThrottlingURLLoaderInDelegateCall_Response) {
  base::RunLoop run_loop1;
  throttle_->set_will_process_response_callback(base::Bind(
      [](const base::Closure& quit_closure,
         URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  client_.set_on_received_response_callback(base::Bind(
      [](ThrottlingURLLoaderTest* test, const base::Closure& quit_closure) {
        // Destroy the ThrottlingURLLoader while inside a delegate call from a
        // throttle.
        test->loader().reset();

        // The throttle should stay alive.
        EXPECT_NE(nullptr, test->throttle());

        quit_closure.Run();
      },
      base::Unretained(this), run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  throttle_->delegate()->Resume();
  run_loop2.Run();

  // The ThrottlingURLLoader should be gone.
  EXPECT_EQ(nullptr, loader_);
  // The throttle should stay alive and destroyed later.
  EXPECT_NE(nullptr, throttle_);

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(nullptr, throttle_);
}

// Regression test for crbug.com/833292.
TEST_F(ThrottlingURLLoaderTest,
       DestroyingThrottlingURLLoaderInDelegateCall_Redirect) {
  base::RunLoop run_loop1;
  throttle_->set_will_redirect_request_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         URLLoaderThrottle::Delegate* delegate, bool* defer,
         std::vector<std::string>* /* removed_headers */,
         net::HttpRequestHeaders* /* modified_headers */) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  client_.set_on_received_redirect_callback(base::BindRepeating(
      [](ThrottlingURLLoaderTest* test,
         const base::RepeatingClosure& quit_closure) {
        // Destroy the ThrottlingURLLoader while inside a delegate call from a
        // throttle.
        test->loader().reset();

        // The throttle should stay alive.
        EXPECT_NE(nullptr, test->throttle());

        quit_closure.Run();
      },
      base::Unretained(this), run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveRedirect();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  throttle_->delegate()->Resume();
  run_loop2.Run();

  // The ThrottlingURLLoader should be gone.
  EXPECT_EQ(nullptr, loader_);
  // The throttle should stay alive and destroyed later.
  EXPECT_NE(nullptr, throttle_);

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(nullptr, throttle_);
}

// Call RestartWithFlags() from a single throttle while processing
// BeforeWillProcessResponse().
TEST_F(ThrottlingURLLoaderTest, RestartWithFlags) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Restart the request when processing BeforeWillProcessResponse(), using
  // different load flags (1).
  throttle_->set_before_will_process_response_callback(
      base::BindRepeating([](URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(1); }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  throttle_->set_before_will_process_response_callback(
      TestURLLoaderThrottle::ThrottleCallback());

  client_.set_on_complete_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int error) {
        EXPECT_EQ(net::OK, error);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
}

// Call RestartWithFlags() from a single throttle after having deferred
// BeforeWillProcessResponse().
TEST_F(ThrottlingURLLoaderTest, DeferThenRestartWithFlags) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Defer BeforeWillProcessResponse().
  throttle_->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  throttle_->delegate()->RestartWithFlags(1);
  throttle_->delegate()->Resume();

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  throttle_->set_before_will_process_response_callback(
      TestURLLoaderThrottle::ThrottleCallback());

  client_.set_on_complete_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int error) {
        EXPECT_EQ(net::OK, error);
        quit_closure.Run();
      },
      run_loop4.QuitClosure()));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
}

// Call RestartWithFlags() from a multiple throttles while processing
// BeforeWillProcessResponse(). Ensures that the request is restarted exactly
// once, using the combination of all additional load flags.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartWithFlags) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }
  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Have two of the three throttles restart whe processing
  // BeforeWillProcessResponse(), using
  // different load flags (2 and 8).
  throttles[0]->set_before_will_process_response_callback(
      base::BindRepeating([](URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(2); }));
  throttles[2]->set_before_will_process_response_callback(
      base::BindRepeating([](URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(8); }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 10 = (2 | 8)).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(10, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int error) {
        EXPECT_EQ(net::OK, error);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
  }
}

// Call RestartWithFlags() from multiple throttles after having deferred
// BeforeWillProcessResponse() in each. Ensures that the request is started
// exactly once, using the combination of all additional load flags.
TEST_F(ThrottlingURLLoaderTest, MultipleDeferThenRestartWithFlags) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Have all of the throttles defer. Once they have all been deferred, quit
  // run_loop2.
  int throttle_counter = 0;
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  for (auto* throttle : throttles) {
    throttle->delegate()->RestartWithFlags(next_load_flag);
    throttle->delegate()->Resume();
    next_load_flag <<= 1;
  }

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int error) {
        EXPECT_EQ(net::OK, error);
        quit_closure.Run();
      },
      run_loop4.QuitClosure()));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
  }
}

// Call RestartWithFlags() from multiple throttles -- two while deferred, and
// one while processing BeforeWillProcessResponse(). Ensures that the request is
// restarted exactly once, using the combination of all additional load flags.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartWithFlagsDeferAndSync) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Have two of the throttles defer, and one call restart
  // synchronously. Once all are run, quit run_loop2.
  int throttle_counter = 0;
  for (size_t i = 0; i < 2u; ++i) {
    throttles[i]->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }
  throttles[2]->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int* count,
         URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithFlags(4);
        if (++(*count) == 3) {
          quit_closure.Run();
        }
      },
      run_loop2.QuitClosure(), &throttle_counter));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  for (auto* throttle : throttles) {
    throttle->delegate()->RestartWithFlags(next_load_flag);
    throttle->delegate()->Resume();
    next_load_flag <<= 1;
  }

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int error) {
        EXPECT_EQ(net::OK, error);
        quit_closure.Run();
      },
      run_loop4.QuitClosure()));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
  }
}

}  // namespace
}  // namespace content
