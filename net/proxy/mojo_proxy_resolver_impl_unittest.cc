// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_impl.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/proxy/mock_proxy_resolver.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
namespace {

class TestRequestClient : public interfaces::ProxyResolverRequestClient,
                          public mojo::ErrorHandler {
 public:
  enum Event {
    RESULT_RECEIVED,
    LOAD_STATE_CHANGED,
    CONNECTION_ERROR,
  };

  explicit TestRequestClient(
      mojo::InterfaceRequest<interfaces::ProxyResolverRequestClient> request);

  void WaitForResult();

  Error error() { return error_; }
  const mojo::Array<interfaces::ProxyServerPtr>& results() { return results_; }
  LoadState load_state() { return load_state_; }
  EventWaiter<Event>& event_waiter() { return event_waiter_; }

 private:
  // interfaces::ProxyResolverRequestClient override.
  void ReportResult(int32_t error,
                    mojo::Array<interfaces::ProxyServerPtr> results) override;
  void LoadStateChanged(int32_t load_state) override;

  // mojo::ErrorHandler override.
  void OnConnectionError() override;

  bool done_ = false;
  Error error_ = ERR_FAILED;
  LoadState load_state_ = LOAD_STATE_IDLE;
  mojo::Array<interfaces::ProxyServerPtr> results_;

  mojo::Binding<interfaces::ProxyResolverRequestClient> binding_;

  EventWaiter<Event> event_waiter_;
};

TestRequestClient::TestRequestClient(
    mojo::InterfaceRequest<interfaces::ProxyResolverRequestClient> request)
    : binding_(this, request.Pass()) {
  binding_.set_error_handler(this);
}

void TestRequestClient::WaitForResult() {
  if (done_)
    return;

  event_waiter_.WaitForEvent(RESULT_RECEIVED);
  ASSERT_TRUE(done_);
}

void TestRequestClient::ReportResult(
    int32_t error,
    mojo::Array<interfaces::ProxyServerPtr> results) {
  event_waiter_.NotifyEvent(RESULT_RECEIVED);
  ASSERT_FALSE(done_);
  error_ = static_cast<Error>(error);
  results_ = results.Pass();
  done_ = true;
}

void TestRequestClient::LoadStateChanged(int32_t load_state) {
  event_waiter_.NotifyEvent(LOAD_STATE_CHANGED);
  load_state_ = static_cast<LoadState>(load_state);
}

void TestRequestClient::OnConnectionError() {
  event_waiter_.NotifyEvent(CONNECTION_ERROR);
}

class SetPacScriptClient {
 public:
  base::Callback<void(int32_t)> CreateCallback();
  Error error() { return error_; }

 private:
  void ReportResult(int32_t error);

  Error error_ = ERR_FAILED;
};

base::Callback<void(int32_t)> SetPacScriptClient::CreateCallback() {
  return base::Bind(&SetPacScriptClient::ReportResult, base::Unretained(this));
}

void SetPacScriptClient::ReportResult(int32_t error) {
  error_ = static_cast<Error>(error);
}

class CallbackMockProxyResolver : public MockAsyncProxyResolverExpectsBytes {
 public:
  CallbackMockProxyResolver() {}
  ~CallbackMockProxyResolver() override;

  // MockAsyncProxyResolverExpectsBytes overrides.
  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const net::CompletionCallback& callback,
                     RequestHandle* request_handle,
                     const BoundNetLog& net_log) override;
  void CancelRequest(RequestHandle request_handle) override;
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& script_data,
                   const net::CompletionCallback& callback) override;

  // Wait until the mock resolver has received a CancelRequest call.
  void WaitForCancel();

  // Queues a proxy result to be returned synchronously.
  void ReturnProxySynchronously(const ProxyInfo& result);

  // Queues a SetPacScript to be completed synchronously.
  void CompleteSetPacScriptSynchronously();

 private:
  base::Closure cancel_callback_;
  scoped_ptr<ProxyInfo> sync_result_;
  bool set_pac_script_sync_ = false;
};

CallbackMockProxyResolver::~CallbackMockProxyResolver() {
  EXPECT_TRUE(pending_requests().empty());
}

int CallbackMockProxyResolver::GetProxyForURL(
    const GURL& url,
    ProxyInfo* results,
    const net::CompletionCallback& callback,
    RequestHandle* request_handle,
    const BoundNetLog& net_log) {
  if (sync_result_) {
    *results = *sync_result_;
    sync_result_.reset();
    return OK;
  }
  return MockAsyncProxyResolverExpectsBytes::GetProxyForURL(
      url, results, callback, request_handle, net_log);
}

void CallbackMockProxyResolver::CancelRequest(RequestHandle request_handle) {
  MockAsyncProxyResolverExpectsBytes::CancelRequest(request_handle);
  if (!cancel_callback_.is_null()) {
    cancel_callback_.Run();
    cancel_callback_.Reset();
  }
}

int CallbackMockProxyResolver::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const net::CompletionCallback& callback) {
  if (set_pac_script_sync_) {
    set_pac_script_sync_ = false;
    return OK;
  }
  return MockAsyncProxyResolverExpectsBytes::SetPacScript(script_data,
                                                          callback);
}

void CallbackMockProxyResolver::WaitForCancel() {
  while (cancelled_requests().empty()) {
    base::RunLoop run_loop;
    cancel_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void CallbackMockProxyResolver::ReturnProxySynchronously(
    const ProxyInfo& result) {
  sync_result_.reset(new ProxyInfo(result));
}

void CallbackMockProxyResolver::CompleteSetPacScriptSynchronously() {
  set_pac_script_sync_ = true;
}

void Fail(int32_t error) {
  FAIL() << "Unexpected callback with error: " << error;
}

}  // namespace

class MojoProxyResolverImplTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_ptr<CallbackMockProxyResolver> mock_resolver(
        new CallbackMockProxyResolver);
    mock_proxy_resolver_ = mock_resolver.get();
    resolver_impl_.reset(new MojoProxyResolverImpl(mock_resolver.Pass()));
    resolver_ = resolver_impl_.get();
  }

  CallbackMockProxyResolver* mock_proxy_resolver_;

  scoped_ptr<MojoProxyResolverImpl> resolver_impl_;
  interfaces::ProxyResolver* resolver_;
};

TEST_F(MojoProxyResolverImplTest, GetProxyForUrl) {
  interfaces::ProxyResolverRequestClientPtr client_ptr;
  TestRequestClient client(mojo::GetProxy(&client_ptr));

  resolver_->GetProxyForUrl("http://example.com", client_ptr.Pass());
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_requests().size());
  scoped_refptr<MockAsyncProxyResolverBase::Request> request =
      mock_proxy_resolver_->pending_requests()[0];
  EXPECT_EQ(GURL("http://example.com"), request->url());

  resolver_impl_->LoadStateChanged(request.get(),
                                   LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
  client.event_waiter().WaitForEvent(TestRequestClient::LOAD_STATE_CHANGED);
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT, client.load_state());

  request->results()->UsePacString(
      "PROXY proxy.example.com:1; "
      "SOCKS4 socks4.example.com:2; "
      "SOCKS5 socks5.example.com:3; "
      "HTTPS https.example.com:4; "
      "QUIC quic.example.com:65000; "
      "DIRECT");
  request->CompleteNow(OK);
  client.WaitForResult();

  EXPECT_EQ(net::OK, client.error());
  std::vector<net::ProxyServer> servers =
      client.results().To<std::vector<net::ProxyServer>>();
  ASSERT_EQ(6u, servers.size());
  EXPECT_EQ(ProxyServer::SCHEME_HTTP, servers[0].scheme());
  EXPECT_EQ("proxy.example.com", servers[0].host_port_pair().host());
  EXPECT_EQ(1, servers[0].host_port_pair().port());

  EXPECT_EQ(ProxyServer::SCHEME_SOCKS4, servers[1].scheme());
  EXPECT_EQ("socks4.example.com", servers[1].host_port_pair().host());
  EXPECT_EQ(2, servers[1].host_port_pair().port());

  EXPECT_EQ(ProxyServer::SCHEME_SOCKS5, servers[2].scheme());
  EXPECT_EQ("socks5.example.com", servers[2].host_port_pair().host());
  EXPECT_EQ(3, servers[2].host_port_pair().port());

  EXPECT_EQ(ProxyServer::SCHEME_HTTPS, servers[3].scheme());
  EXPECT_EQ("https.example.com", servers[3].host_port_pair().host());
  EXPECT_EQ(4, servers[3].host_port_pair().port());

  EXPECT_EQ(ProxyServer::SCHEME_QUIC, servers[4].scheme());
  EXPECT_EQ("quic.example.com", servers[4].host_port_pair().host());
  EXPECT_EQ(65000, servers[4].host_port_pair().port());

  EXPECT_EQ(ProxyServer::SCHEME_DIRECT, servers[5].scheme());
}

TEST_F(MojoProxyResolverImplTest, GetProxyForUrlSynchronous) {
  interfaces::ProxyResolverRequestClientPtr client_ptr;
  TestRequestClient client(mojo::GetProxy(&client_ptr));

  ProxyInfo result;
  result.UsePacString("DIRECT");
  mock_proxy_resolver_->ReturnProxySynchronously(result);
  resolver_->GetProxyForUrl("http://example.com", client_ptr.Pass());
  ASSERT_EQ(0u, mock_proxy_resolver_->pending_requests().size());
  client.WaitForResult();

  EXPECT_EQ(net::OK, client.error());
  std::vector<net::ProxyServer> proxy_servers =
      client.results().To<std::vector<net::ProxyServer>>();
  ASSERT_EQ(1u, proxy_servers.size());
  net::ProxyServer& server = proxy_servers[0];
  EXPECT_TRUE(server.is_direct());
}

TEST_F(MojoProxyResolverImplTest, GetProxyForUrlFailure) {
  interfaces::ProxyResolverRequestClientPtr client_ptr;
  TestRequestClient client(mojo::GetProxy(&client_ptr));

  resolver_->GetProxyForUrl("http://example.com", client_ptr.Pass());
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_requests().size());
  scoped_refptr<MockAsyncProxyResolverBase::Request> request =
      mock_proxy_resolver_->pending_requests()[0];
  EXPECT_EQ(GURL("http://example.com"), request->url());
  request->CompleteNow(ERR_FAILED);
  client.WaitForResult();

  EXPECT_EQ(ERR_FAILED, client.error());
  std::vector<net::ProxyServer> proxy_servers =
      client.results().To<std::vector<net::ProxyServer>>();
  EXPECT_TRUE(proxy_servers.empty());
}

TEST_F(MojoProxyResolverImplTest, GetProxyForUrlMultiple) {
  interfaces::ProxyResolverRequestClientPtr client_ptr1;
  TestRequestClient client1(mojo::GetProxy(&client_ptr1));
  interfaces::ProxyResolverRequestClientPtr client_ptr2;
  TestRequestClient client2(mojo::GetProxy(&client_ptr2));

  resolver_->GetProxyForUrl("http://example.com", client_ptr1.Pass());
  resolver_->GetProxyForUrl("https://example.com", client_ptr2.Pass());
  ASSERT_EQ(2u, mock_proxy_resolver_->pending_requests().size());
  scoped_refptr<MockAsyncProxyResolverBase::Request> request1 =
      mock_proxy_resolver_->pending_requests()[0];
  EXPECT_EQ(GURL("http://example.com"), request1->url());
  scoped_refptr<MockAsyncProxyResolverBase::Request> request2 =
      mock_proxy_resolver_->pending_requests()[1];
  EXPECT_EQ(GURL("https://example.com"), request2->url());
  request1->results()->UsePacString("HTTPS proxy.example.com:12345");
  request1->CompleteNow(OK);
  request2->results()->UsePacString("SOCKS5 another-proxy.example.com:6789");
  request2->CompleteNow(OK);
  client1.WaitForResult();
  client2.WaitForResult();

  EXPECT_EQ(net::OK, client1.error());
  std::vector<net::ProxyServer> proxy_servers1 =
      client1.results().To<std::vector<net::ProxyServer>>();
  ASSERT_EQ(1u, proxy_servers1.size());
  net::ProxyServer& server1 = proxy_servers1[0];
  EXPECT_EQ(ProxyServer::SCHEME_HTTPS, server1.scheme());
  EXPECT_EQ("proxy.example.com", server1.host_port_pair().host());
  EXPECT_EQ(12345, server1.host_port_pair().port());

  EXPECT_EQ(net::OK, client2.error());
  std::vector<net::ProxyServer> proxy_servers2 =
      client2.results().To<std::vector<net::ProxyServer>>();
  ASSERT_EQ(1u, proxy_servers1.size());
  net::ProxyServer& server2 = proxy_servers2[0];
  EXPECT_EQ(ProxyServer::SCHEME_SOCKS5, server2.scheme());
  EXPECT_EQ("another-proxy.example.com", server2.host_port_pair().host());
  EXPECT_EQ(6789, server2.host_port_pair().port());
}

TEST_F(MojoProxyResolverImplTest, SetPacScript) {
  SetPacScriptClient client;

  resolver_->SetPacScript("pac script", client.CreateCallback());
  MockAsyncProxyResolverBase::SetPacScriptRequest* request =
      mock_proxy_resolver_->pending_set_pac_script_request();
  ASSERT_TRUE(request);
  EXPECT_EQ("pac script", base::UTF16ToUTF8(request->script_data()->utf16()));
  request->CompleteNow(OK);
  EXPECT_EQ(OK, client.error());
}

TEST_F(MojoProxyResolverImplTest, SetPacScriptSynchronous) {
  SetPacScriptClient client;

  mock_proxy_resolver_->CompleteSetPacScriptSynchronously();
  resolver_->SetPacScript("pac script", client.CreateCallback());
  EXPECT_FALSE(mock_proxy_resolver_->pending_set_pac_script_request());
  EXPECT_EQ(OK, client.error());
}

TEST_F(MojoProxyResolverImplTest, SetPacScriptMultiple) {
  SetPacScriptClient client1;
  SetPacScriptClient client2;

  resolver_->SetPacScript("pac script", client1.CreateCallback());
  resolver_->SetPacScript("a different pac script", client2.CreateCallback());
  MockAsyncProxyResolverBase::SetPacScriptRequest* request =
      mock_proxy_resolver_->pending_set_pac_script_request();
  ASSERT_TRUE(request);
  EXPECT_EQ("pac script", base::UTF16ToUTF8(request->script_data()->utf16()));
  request->CompleteNow(OK);
  EXPECT_EQ(OK, client1.error());

  request = mock_proxy_resolver_->pending_set_pac_script_request();
  ASSERT_TRUE(request);
  EXPECT_EQ("a different pac script",
            base::UTF16ToUTF8(request->script_data()->utf16()));
  request->CompleteNow(ERR_PAC_SCRIPT_FAILED);
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, client2.error());
}

TEST_F(MojoProxyResolverImplTest, DestroyClient) {
  interfaces::ProxyResolverRequestClientPtr client_ptr;
  scoped_ptr<TestRequestClient> client(
      new TestRequestClient(mojo::GetProxy(&client_ptr)));

  resolver_->GetProxyForUrl("http://example.com", client_ptr.Pass());
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_requests().size());
  scoped_refptr<MockAsyncProxyResolverBase::Request> request =
      mock_proxy_resolver_->pending_requests()[0];
  EXPECT_EQ(GURL("http://example.com"), request->url());
  request->results()->UsePacString("PROXY proxy.example.com:8080");
  client.reset();
  mock_proxy_resolver_->WaitForCancel();
}

TEST_F(MojoProxyResolverImplTest, DestroyService) {
  interfaces::ProxyResolverRequestClientPtr client_ptr;
  TestRequestClient client(mojo::GetProxy(&client_ptr));

  resolver_->GetProxyForUrl("http://example.com", client_ptr.Pass());
  resolver_->SetPacScript("pac script", base::Bind(&Fail));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_requests().size());
  scoped_refptr<MockAsyncProxyResolverBase::Request> request =
      mock_proxy_resolver_->pending_requests()[0];
  resolver_impl_.reset();
  client.event_waiter().WaitForEvent(TestRequestClient::CONNECTION_ERROR);
}

}  // namespace net
