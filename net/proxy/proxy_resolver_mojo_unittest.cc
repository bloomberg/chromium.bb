// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_mojo.h"

#include <list>
#include <map>
#include <queue>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "mojo/common/common_type_converters.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "net/proxy/proxy_resolver_script_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kScriptData[] = "FooBarBaz";
const char kExampleUrl[] = "http://www.example.com";

struct CreateProxyResolverAction {
  enum Action {
    COMPLETE,
    DROP_CLIENT,
    DROP_RESOLVER,
    DROP_BOTH,
    WAIT_FOR_CLIENT_DISCONNECT,
  };

  static CreateProxyResolverAction ReturnResult(
      const std::string& expected_pac_script,
      Error error) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.error = error;
    return result;
  }

  static CreateProxyResolverAction DropClient(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = DROP_CLIENT;
    return result;
  }

  static CreateProxyResolverAction DropResolver(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = DROP_RESOLVER;
    return result;
  }

  static CreateProxyResolverAction DropBoth(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = DROP_BOTH;
    return result;
  }

  static CreateProxyResolverAction WaitForClientDisconnect(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = WAIT_FOR_CLIENT_DISCONNECT;
    return result;
  }

  std::string expected_pac_script;
  Action action = COMPLETE;
  Error error = OK;
};

struct GetProxyForUrlAction {
  enum Action {
    COMPLETE,
    // Drop the request by closing the reply channel.
    DROP,
    // Disconnect the service.
    DISCONNECT,
    // Wait for the client pipe to be disconnected.
    WAIT_FOR_CLIENT_DISCONNECT,
    // Send a LoadStateChanged message and keep the client pipe open.
    SEND_LOAD_STATE_AND_BLOCK,
  };

  GetProxyForUrlAction() {}
  GetProxyForUrlAction(const GetProxyForUrlAction& old) {
    action = old.action;
    error = old.error;
    expected_url = old.expected_url;
    proxy_servers = old.proxy_servers.Clone();
  }

  static GetProxyForUrlAction ReturnError(const GURL& url, Error error) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.error = error;
    return result;
  }

  static GetProxyForUrlAction ReturnServers(
      const GURL& url,
      const mojo::Array<interfaces::ProxyServerPtr>& proxy_servers) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.proxy_servers = proxy_servers.Clone();
    return result;
  }

  static GetProxyForUrlAction DropRequest(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = DROP;
    return result;
  }

  static GetProxyForUrlAction Disconnect(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = DISCONNECT;
    return result;
  }

  static GetProxyForUrlAction WaitForClientDisconnect(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = WAIT_FOR_CLIENT_DISCONNECT;
    return result;
  }

  static GetProxyForUrlAction SendLoadStateChanged(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = SEND_LOAD_STATE_AND_BLOCK;
    return result;
  }

  Action action = COMPLETE;
  Error error = OK;
  mojo::Array<interfaces::ProxyServerPtr> proxy_servers;
  GURL expected_url;
};

class MockMojoProxyResolver : public interfaces::ProxyResolver {
 public:
  MockMojoProxyResolver();
  ~MockMojoProxyResolver() override;

  void AddGetProxyAction(GetProxyForUrlAction action);

  void WaitForNextRequest();

  void ClearBlockedClients();

  void AddConnection(mojo::InterfaceRequest<interfaces::ProxyResolver> req);

 private:
  // Overridden from interfaces::ProxyResolver:
  void GetProxyForUrl(
      const mojo::String& url,
      interfaces::ProxyResolverRequestClientPtr client) override;

  void WakeWaiter();

  std::string pac_script_data_;

  std::queue<GetProxyForUrlAction> get_proxy_actions_;

  base::Closure quit_closure_;

  ScopedVector<interfaces::ProxyResolverRequestClientPtr> blocked_clients_;
  mojo::Binding<interfaces::ProxyResolver> binding_;
};

MockMojoProxyResolver::~MockMojoProxyResolver() {
  EXPECT_TRUE(get_proxy_actions_.empty())
      << "Actions remaining: " << get_proxy_actions_.size();
}

MockMojoProxyResolver::MockMojoProxyResolver() : binding_(this) {
}

void MockMojoProxyResolver::AddGetProxyAction(GetProxyForUrlAction action) {
  get_proxy_actions_.push(action);
}

void MockMojoProxyResolver::WaitForNextRequest() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockMojoProxyResolver::WakeWaiter() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
  quit_closure_.Reset();
}

void MockMojoProxyResolver::ClearBlockedClients() {
  blocked_clients_.clear();
}

void MockMojoProxyResolver::AddConnection(
    mojo::InterfaceRequest<interfaces::ProxyResolver> req) {
  if (binding_.is_bound())
    binding_.Close();
  binding_.Bind(req.Pass());
}

void MockMojoProxyResolver::GetProxyForUrl(
    const mojo::String& url,
    interfaces::ProxyResolverRequestClientPtr client) {
  ASSERT_FALSE(get_proxy_actions_.empty());
  GetProxyForUrlAction action = get_proxy_actions_.front();
  get_proxy_actions_.pop();

  EXPECT_EQ(action.expected_url.spec(), url.To<std::string>());
  switch (action.action) {
    case GetProxyForUrlAction::COMPLETE:
      client->ReportResult(action.error, action.proxy_servers.Pass());
      break;
    case GetProxyForUrlAction::DROP:
      client.reset();
      break;
    case GetProxyForUrlAction::DISCONNECT:
      binding_.Close();
      break;
    case GetProxyForUrlAction::WAIT_FOR_CLIENT_DISCONNECT:
      ASSERT_FALSE(client.WaitForIncomingMethodCall());
      break;
    case GetProxyForUrlAction::SEND_LOAD_STATE_AND_BLOCK:
      client->LoadStateChanged(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
      blocked_clients_.push_back(
          new interfaces::ProxyResolverRequestClientPtr(client.Pass()));
      break;
  }
  WakeWaiter();
}

class Request {
 public:
  Request(ProxyResolver* resolver, const GURL& url);

  int Resolve();
  void Cancel();
  int WaitForResult();

  int error() const { return error_; }
  const ProxyInfo& results() const { return results_; }
  LoadState load_state() { return resolver_->GetLoadState(handle_); }

 private:
  ProxyResolver* resolver_;
  const GURL url_;
  ProxyInfo results_;
  ProxyResolver::RequestHandle handle_;
  int error_;
  TestCompletionCallback callback_;
};

Request::Request(ProxyResolver* resolver, const GURL& url)
    : resolver_(resolver), url_(url), error_(0) {
}

int Request::Resolve() {
  BoundNetLog net_log;
  error_ = resolver_->GetProxyForURL(url_, &results_, callback_.callback(),
                                     &handle_, net_log);
  return error_;
}

void Request::Cancel() {
  resolver_->CancelRequest(handle_);
}

int Request::WaitForResult() {
  error_ = callback_.WaitForResult();
  return error_;
}

class MockMojoProxyResolverFactory : public interfaces::ProxyResolverFactory {
 public:
  MockMojoProxyResolverFactory(
      MockMojoProxyResolver* resolver,
      mojo::InterfaceRequest<interfaces::ProxyResolverFactory> req);
  ~MockMojoProxyResolverFactory() override;

  void AddCreateProxyResolverAction(CreateProxyResolverAction action);

  void WaitForNextRequest();

  void ClearBlockedClients();

 private:
  // Overridden from interfaces::ProxyResolver:
  void CreateResolver(
      const mojo::String& pac_url,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request,
      interfaces::HostResolverPtr host_resolver,
      interfaces::ProxyResolverErrorObserverPtr error_observer,
      interfaces::ProxyResolverFactoryRequestClientPtr client) override;

  void WakeWaiter();

  MockMojoProxyResolver* resolver_;
  std::queue<CreateProxyResolverAction> create_resolver_actions_;

  base::Closure quit_closure_;

  ScopedVector<interfaces::ProxyResolverFactoryRequestClientPtr>
      blocked_clients_;
  ScopedVector<mojo::InterfaceRequest<interfaces::ProxyResolver>>
      blocked_resolver_requests_;
  mojo::Binding<interfaces::ProxyResolverFactory> binding_;
};

MockMojoProxyResolverFactory::MockMojoProxyResolverFactory(
    MockMojoProxyResolver* resolver,
    mojo::InterfaceRequest<interfaces::ProxyResolverFactory> req)
    : resolver_(resolver), binding_(this, req.Pass()) {
}

MockMojoProxyResolverFactory::~MockMojoProxyResolverFactory() {
  EXPECT_TRUE(create_resolver_actions_.empty())
      << "Actions remaining: " << create_resolver_actions_.size();
}

void MockMojoProxyResolverFactory::AddCreateProxyResolverAction(
    CreateProxyResolverAction action) {
  create_resolver_actions_.push(action);
}

void MockMojoProxyResolverFactory::WaitForNextRequest() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockMojoProxyResolverFactory::WakeWaiter() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
  quit_closure_.Reset();
}

void MockMojoProxyResolverFactory::ClearBlockedClients() {
  blocked_clients_.clear();
}

void MockMojoProxyResolverFactory::CreateResolver(
    const mojo::String& pac_script,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::HostResolverPtr host_resolver,
    interfaces::ProxyResolverErrorObserverPtr error_observer,
    interfaces::ProxyResolverFactoryRequestClientPtr client) {
  ASSERT_FALSE(create_resolver_actions_.empty());
  CreateProxyResolverAction action = create_resolver_actions_.front();
  create_resolver_actions_.pop();

  EXPECT_EQ(action.expected_pac_script, pac_script.To<std::string>());
  switch (action.action) {
    case CreateProxyResolverAction::COMPLETE:
      if (action.error == OK)
        resolver_->AddConnection(request.Pass());
      client->ReportResult(action.error);
      break;
    case CreateProxyResolverAction::DROP_CLIENT:
      // Save |request| so its pipe isn't closed.
      blocked_resolver_requests_.push_back(
          new mojo::InterfaceRequest<interfaces::ProxyResolver>(
              request.Pass()));
      break;
    case CreateProxyResolverAction::DROP_RESOLVER:
      // Save |client| so its pipe isn't closed.
      blocked_clients_.push_back(
          new interfaces::ProxyResolverFactoryRequestClientPtr(client.Pass()));
      break;
    case CreateProxyResolverAction::DROP_BOTH:
      // Both |request| and |client| will be closed.
      break;
    case CreateProxyResolverAction::WAIT_FOR_CLIENT_DISCONNECT:
      ASSERT_FALSE(client.WaitForIncomingMethodCall());
      break;
  }
  WakeWaiter();
}

}  // namespace

class ProxyResolverMojoTest : public testing::Test,
                              public MojoProxyResolverFactory {
 public:
  void SetUp() override {
    mock_proxy_resolver_factory_.reset(new MockMojoProxyResolverFactory(
        &mock_proxy_resolver_, mojo::GetProxy(&factory_ptr_)));
    proxy_resolver_factory_mojo_.reset(new ProxyResolverFactoryMojo(
        this, nullptr,
        base::Callback<scoped_ptr<ProxyResolverErrorObserver>()>()));
  }

  scoped_ptr<Request> MakeRequest(const GURL& url) {
    return make_scoped_ptr(new Request(proxy_resolver_mojo_.get(), url));
  }

  scoped_ptr<base::ScopedClosureRunner> CreateResolver(
      const mojo::String& pac_script,
      mojo::InterfaceRequest<interfaces::ProxyResolver> req,
      interfaces::HostResolverPtr host_resolver,
      interfaces::ProxyResolverErrorObserverPtr error_observer,
      interfaces::ProxyResolverFactoryRequestClientPtr client) override {
    factory_ptr_->CreateResolver(pac_script, req.Pass(), host_resolver.Pass(),
                                 error_observer.Pass(), client.Pass());
    return make_scoped_ptr(
        new base::ScopedClosureRunner(on_delete_callback_.closure()));
  }

  mojo::Array<interfaces::ProxyServerPtr> ProxyServersFromPacString(
      const std::string& pac_string) {
    ProxyInfo proxy_info;
    proxy_info.UsePacString(pac_string);

    return mojo::Array<interfaces::ProxyServerPtr>::From(
        proxy_info.proxy_list().GetAll());
  }

  void CreateProxyResolver() {
    mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
        CreateProxyResolverAction::ReturnResult(kScriptData, OK));
    TestCompletionCallback callback;
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData));
    scoped_ptr<ProxyResolverFactory::Request> request;
    ASSERT_EQ(
        OK,
        callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
            pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
    EXPECT_TRUE(request);
    ASSERT_TRUE(proxy_resolver_mojo_);
  }

  void DeleteProxyResolverCallback(const CompletionCallback& callback,
                                   int result) {
    proxy_resolver_mojo_.reset();
    callback.Run(result);
  }

  scoped_ptr<MockMojoProxyResolverFactory> mock_proxy_resolver_factory_;
  interfaces::ProxyResolverFactoryPtr factory_ptr_;
  scoped_ptr<ProxyResolverFactory> proxy_resolver_factory_mojo_;

  MockMojoProxyResolver mock_proxy_resolver_;
  scoped_ptr<ProxyResolver> proxy_resolver_mojo_;
  TestClosure on_delete_callback_;
};

TEST_F(ProxyResolverMojoTest, CreateProxyResolver) {
  CreateProxyResolver();
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_Empty) {
  TestCompletionCallback callback;
  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(""));
  scoped_ptr<ProxyResolverFactory::Request> request;
  EXPECT_EQ(
      ERR_PAC_SCRIPT_FAILED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_FALSE(request);
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_Url) {
  TestCompletionCallback callback;
  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromURL(GURL(kExampleUrl)));
  scoped_ptr<ProxyResolverFactory::Request> request;
  EXPECT_EQ(
      ERR_PAC_SCRIPT_FAILED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_FALSE(request);
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_Failed) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::ReturnResult(kScriptData,
                                              ERR_PAC_STATUS_NOT_OK));

  TestCompletionCallback callback;
  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  scoped_ptr<ProxyResolverFactory::Request> request;
  EXPECT_EQ(
      ERR_PAC_STATUS_NOT_OK,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);
  on_delete_callback_.WaitForResult();

  // A second attempt succeeds.
  CreateProxyResolver();
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_BothDisconnected) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropBoth(kScriptData));

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  scoped_ptr<ProxyResolverFactory::Request> request;
  TestCompletionCallback callback;
  EXPECT_EQ(
      ERR_PAC_SCRIPT_TERMINATED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_ClientDisconnected) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropClient(kScriptData));

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  scoped_ptr<ProxyResolverFactory::Request> request;
  TestCompletionCallback callback;
  EXPECT_EQ(
      ERR_PAC_SCRIPT_TERMINATED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_ResolverDisconnected) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropResolver(kScriptData));

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  scoped_ptr<ProxyResolverFactory::Request> request;
  TestCompletionCallback callback;
  EXPECT_EQ(
      ERR_PAC_SCRIPT_TERMINATED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);
  on_delete_callback_.WaitForResult();
}

TEST_F(ProxyResolverMojoTest, CreateProxyResolver_Cancel) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::WaitForClientDisconnect(kScriptData));

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  scoped_ptr<ProxyResolverFactory::Request> request;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_factory_mojo_->CreateProxyResolver(
                                pac_script, &proxy_resolver_mojo_,
                                callback.callback(), &request));
  ASSERT_TRUE(request);
  request.reset();

  // The Mojo request is still made.
  mock_proxy_resolver_factory_->WaitForNextRequest();
  on_delete_callback_.WaitForResult();
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL) {
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  CreateProxyResolver();

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(OK, request->WaitForResult());

  EXPECT_EQ("DIRECT", request->results().ToPacString());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_LoadState) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::SendLoadStateChanged(GURL(kExampleUrl)));
  CreateProxyResolver();

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request->load_state());
  while (request->load_state() == LOAD_STATE_RESOLVING_PROXY_FOR_URL)
    base::RunLoop().RunUntilIdle();
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT, request->load_state());
  mock_proxy_resolver_.ClearBlockedClients();
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->WaitForResult());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_MultipleResults) {
  static const char kPacString[] =
      "PROXY foo1:80;DIRECT;SOCKS foo2:1234;"
      "SOCKS5 foo3:1080;HTTPS foo4:443;QUIC foo6:8888";
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString(kPacString)));
  CreateProxyResolver();

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(OK, request->WaitForResult());

  EXPECT_EQ(kPacString, request->results().ToPacString());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_Error) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::ReturnError(GURL(kExampleUrl), ERR_UNEXPECTED));
  CreateProxyResolver();

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(ERR_UNEXPECTED, request->WaitForResult());

  EXPECT_TRUE(request->results().is_empty());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_Cancel) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::WaitForClientDisconnect(GURL(kExampleUrl)));
  CreateProxyResolver();

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  request->Cancel();

  // The Mojo request is still made.
  mock_proxy_resolver_.WaitForNextRequest();
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_MultipleRequests) {
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL("https://www.chromium.org"),
      ProxyServersFromPacString("HTTPS foo:443")));
  CreateProxyResolver();

  scoped_ptr<Request> request1(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request1->Resolve());
  scoped_ptr<Request> request2(MakeRequest(GURL("https://www.chromium.org")));
  EXPECT_EQ(ERR_IO_PENDING, request2->Resolve());

  EXPECT_EQ(OK, request1->WaitForResult());
  EXPECT_EQ(OK, request2->WaitForResult());

  EXPECT_EQ("DIRECT", request1->results().ToPacString());
  EXPECT_EQ("HTTPS foo:443", request2->results().ToPacString());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_Disconnect) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));
  CreateProxyResolver();
  {
    scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->WaitForResult());
    EXPECT_TRUE(request->results().is_empty());
  }

  {
    // Calling GetProxyForURL after a disconnect should fail.
    scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->Resolve());
  }
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_ClientClosed) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::DropRequest(GURL(kExampleUrl)));
  CreateProxyResolver();

  scoped_ptr<Request> request1(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request1->Resolve());

  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request1->WaitForResult());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_DeleteInCallback) {
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  CreateProxyResolver();

  ProxyInfo results;
  TestCompletionCallback callback;
  ProxyResolver::RequestHandle handle;
  BoundNetLog net_log;
  EXPECT_EQ(OK,
            callback.GetResult(proxy_resolver_mojo_->GetProxyForURL(
                GURL(kExampleUrl), &results,
                base::Bind(&ProxyResolverMojoTest::DeleteProxyResolverCallback,
                           base::Unretained(this), callback.callback()),
                &handle, net_log)));
  on_delete_callback_.WaitForResult();
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_DeleteInCallbackFromDisconnect) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));
  CreateProxyResolver();

  ProxyInfo results;
  TestCompletionCallback callback;
  ProxyResolver::RequestHandle handle;
  BoundNetLog net_log;
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED,
            callback.GetResult(proxy_resolver_mojo_->GetProxyForURL(
                GURL(kExampleUrl), &results,
                base::Bind(&ProxyResolverMojoTest::DeleteProxyResolverCallback,
                           base::Unretained(this), callback.callback()),
                &handle, net_log)));
  on_delete_callback_.WaitForResult();
}

TEST_F(ProxyResolverMojoTest, DeleteResolver) {
  CreateProxyResolver();
  proxy_resolver_mojo_.reset();
  on_delete_callback_.WaitForResult();
}
}  // namespace net
