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
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_script_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kScriptData[] = "FooBarBaz";
const char kScriptData2[] = "BlahBlahBlah";
const char kExampleUrl[] = "http://www.example.com";

struct SetPacScriptAction {
  enum Action {
    COMPLETE,
    DISCONNECT,
  };

  static SetPacScriptAction ReturnResult(Error error) {
    SetPacScriptAction result;
    result.error = error;
    return result;
  }

  static SetPacScriptAction Disconnect() {
    SetPacScriptAction result;
    result.action = DISCONNECT;
    return result;
  }

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
  explicit MockMojoProxyResolver(
      mojo::InterfaceRequest<interfaces::ProxyResolver> req);
  ~MockMojoProxyResolver() override;

  void AddPacScriptAction(SetPacScriptAction action);
  // Returned script data is UTF8.
  std::string pac_script_data() { return pac_script_data_; }

  void AddGetProxyAction(GetProxyForUrlAction action);

  void WaitForNextRequest();

  void ClearBlockedClients();

 private:
  // Overridden from interfaces::ProxyResolver:
  void SetPacScript(const mojo::String& data,
                    const mojo::Callback<void(int32_t)>& callback) override;
  void GetProxyForUrl(
      const mojo::String& url,
      interfaces::ProxyResolverRequestClientPtr client) override;

  void WakeWaiter();

  std::string pac_script_data_;
  std::queue<SetPacScriptAction> pac_script_actions_;

  std::queue<GetProxyForUrlAction> get_proxy_actions_;

  base::Closure quit_closure_;

  ScopedVector<interfaces::ProxyResolverRequestClientPtr> blocked_clients_;
  mojo::Binding<interfaces::ProxyResolver> binding_;
};

MockMojoProxyResolver::MockMojoProxyResolver(
    mojo::InterfaceRequest<interfaces::ProxyResolver> req)
    : binding_(this, req.Pass()) {
}

MockMojoProxyResolver::~MockMojoProxyResolver() {
  EXPECT_TRUE(pac_script_actions_.empty())
      << "Actions remaining: " << pac_script_actions_.size();
  EXPECT_TRUE(get_proxy_actions_.empty())
      << "Actions remaining: " << get_proxy_actions_.size();
}

void MockMojoProxyResolver::AddPacScriptAction(SetPacScriptAction action) {
  pac_script_actions_.push(action);
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

void MockMojoProxyResolver::SetPacScript(
    const mojo::String& data,
    const mojo::Callback<void(int32_t)>& callback) {
  pac_script_data_ = data.To<std::string>();

  ASSERT_FALSE(pac_script_actions_.empty());
  SetPacScriptAction action = pac_script_actions_.front();
  pac_script_actions_.pop();

  switch (action.action) {
    case SetPacScriptAction::COMPLETE:
      callback.Run(action.error);
      break;
    case SetPacScriptAction::DISCONNECT:
      binding_.Close();
      break;
  }
  WakeWaiter();
}

void MockMojoProxyResolver::ClearBlockedClients() {
  blocked_clients_.clear();
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

class TestMojoProxyResolverFactory : public MojoProxyResolverFactory {
 public:
  TestMojoProxyResolverFactory();
  ~TestMojoProxyResolverFactory();

  // Overridden from MojoProxyResolverFactory:
  void Create(mojo::InterfaceRequest<interfaces::ProxyResolver> req,
              interfaces::HostResolverPtr host_resolver) override;

  MockMojoProxyResolver& GetMockResolver() { return *mock_proxy_resolver_; }

  void AddFuturePacScriptAction(int creation, SetPacScriptAction action);
  void AddFutureGetProxyAction(int creation, GetProxyForUrlAction action);

  int num_create_calls() const { return num_create_calls_; }
  void FailNextCreate() { fail_next_create_ = true; }

 private:
  int num_create_calls_;
  std::map<int, std::list<SetPacScriptAction>> pac_script_actions_;
  std::map<int, std::list<GetProxyForUrlAction>> get_proxy_actions_;
  bool fail_next_create_;

  scoped_ptr<MockMojoProxyResolver> mock_proxy_resolver_;
};

TestMojoProxyResolverFactory::TestMojoProxyResolverFactory()
    : num_create_calls_(0), fail_next_create_(false) {
}

TestMojoProxyResolverFactory::~TestMojoProxyResolverFactory() {
}

void TestMojoProxyResolverFactory::Create(
    mojo::InterfaceRequest<interfaces::ProxyResolver> req,
    interfaces::HostResolverPtr host_resolver) {
  if (fail_next_create_) {
    req = nullptr;
    fail_next_create_ = false;
  } else {
    mock_proxy_resolver_.reset(new MockMojoProxyResolver(req.Pass()));

    for (const auto& action : pac_script_actions_[num_create_calls_])
      mock_proxy_resolver_->AddPacScriptAction(action);

    for (const auto& action : get_proxy_actions_[num_create_calls_])
      mock_proxy_resolver_->AddGetProxyAction(action);
  }
  num_create_calls_++;
}

void TestMojoProxyResolverFactory::AddFuturePacScriptAction(
    int creation,
    SetPacScriptAction action) {
  pac_script_actions_[creation].push_back(action);
}

void TestMojoProxyResolverFactory::AddFutureGetProxyAction(
    int creation,
    GetProxyForUrlAction action) {
  get_proxy_actions_[creation].push_back(action);
}

class Request {
 public:
  Request(ProxyResolverMojo* resolver, const GURL& url);

  int Resolve();
  void Cancel();
  int WaitForResult();

  int error() const { return error_; }
  const ProxyInfo& results() const { return results_; }
  LoadState load_state() { return resolver_->GetLoadState(handle_); }

 private:
  ProxyResolverMojo* resolver_;
  const GURL url_;
  ProxyInfo results_;
  ProxyResolver::RequestHandle handle_;
  int error_;
  TestCompletionCallback callback_;
};

Request::Request(ProxyResolverMojo* resolver, const GURL& url)
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

}  // namespace

class ProxyResolverMojoTest : public testing::Test {
 public:
  void SetUp() override {
    proxy_resolver_mojo_.reset(new ProxyResolverMojo(
        &mojo_proxy_resolver_factory_, &mock_host_resolver_));
  }

  scoped_ptr<Request> MakeRequest(const GURL& url) {
    return make_scoped_ptr(new Request(proxy_resolver_mojo_.get(), url));
  }

  mojo::Array<interfaces::ProxyServerPtr> ProxyServersFromPacString(
      const std::string& pac_string) {
    ProxyInfo proxy_info;
    proxy_info.UsePacString(pac_string);

    return mojo::Array<interfaces::ProxyServerPtr>::From(
        proxy_info.proxy_list().GetAll());
  }

  void SetPacScript(int instance) {
    mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
        instance, SetPacScriptAction::ReturnResult(OK));
    TestCompletionCallback callback;
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData));
    EXPECT_EQ(OK, callback.GetResult(proxy_resolver_mojo_->SetPacScript(
                      pac_script, callback.callback())));
  }

  void RunCallbackAndSetPacScript(const net::CompletionCallback& callback,
                                  const net::CompletionCallback& pac_callback,
                                  int instance,
                                  int result) {
    callback.Run(result);
    mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
        instance, SetPacScriptAction::ReturnResult(OK));
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData));
    EXPECT_EQ(ERR_IO_PENDING,
              proxy_resolver_mojo_->SetPacScript(pac_script, pac_callback));
  }

  MockHostResolver mock_host_resolver_;
  TestMojoProxyResolverFactory mojo_proxy_resolver_factory_;
  scoped_ptr<ProxyResolverMojo> proxy_resolver_mojo_;
};

TEST_F(ProxyResolverMojoTest, SetPacScript) {
  SetPacScript(0);
  EXPECT_EQ(kScriptData,
            mojo_proxy_resolver_factory_.GetMockResolver().pac_script_data());
}

TEST_F(ProxyResolverMojoTest, SetPacScript_Empty) {
  TestCompletionCallback callback;
  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(""));
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED,
            callback.GetResult(proxy_resolver_mojo_->SetPacScript(
                pac_script, callback.callback())));
}

TEST_F(ProxyResolverMojoTest, SetPacScript_Url) {
  TestCompletionCallback callback;
  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromURL(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED,
            callback.GetResult(proxy_resolver_mojo_->SetPacScript(
                pac_script, callback.callback())));
}

TEST_F(ProxyResolverMojoTest, SetPacScript_Failed) {
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::ReturnResult(ERR_PAC_STATUS_NOT_OK));

  TestCompletionCallback callback;
  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  EXPECT_EQ(ERR_PAC_STATUS_NOT_OK,
            callback.GetResult(proxy_resolver_mojo_->SetPacScript(
                pac_script, callback.callback())));
}

TEST_F(ProxyResolverMojoTest, SetPacScript_Disconnected) {
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::Disconnect());

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_mojo_->SetPacScript(
                                pac_script, callback.callback()));
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, callback.GetResult(ERR_IO_PENDING));
}

TEST_F(ProxyResolverMojoTest, SetPacScript_SuccessThenDisconnect) {
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::ReturnResult(OK));
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::Disconnect());
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      1, SetPacScriptAction::ReturnResult(ERR_FAILED));
  {
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData));
    TestCompletionCallback callback;
    EXPECT_EQ(OK, callback.GetResult(proxy_resolver_mojo_->SetPacScript(
                      pac_script, callback.callback())));
    EXPECT_EQ(kScriptData,
              mojo_proxy_resolver_factory_.GetMockResolver().pac_script_data());
  }

  {
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData2));
    TestCompletionCallback callback;
    EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_mojo_->SetPacScript(
                                  pac_script, callback.callback()));
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, callback.GetResult(ERR_IO_PENDING));
  }

  {
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData2));
    TestCompletionCallback callback;
    EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_mojo_->SetPacScript(
                                  pac_script, callback.callback()));
    EXPECT_EQ(ERR_FAILED, callback.GetResult(ERR_IO_PENDING));
  }

  // The service should have been recreated on the last SetPacScript call.
  EXPECT_EQ(2, mojo_proxy_resolver_factory_.num_create_calls());
}

TEST_F(ProxyResolverMojoTest, SetPacScript_Cancel) {
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::ReturnResult(OK));

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_mojo_->SetPacScript(
                                pac_script, callback.callback()));
  proxy_resolver_mojo_->CancelSetPacScript();

  // The Mojo request is still made.
  mojo_proxy_resolver_factory_.GetMockResolver().WaitForNextRequest();
}

TEST_F(ProxyResolverMojoTest, SetPacScript_CancelAndSetAgain) {
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::ReturnResult(ERR_FAILED));
  mojo_proxy_resolver_factory_.AddFuturePacScriptAction(
      0, SetPacScriptAction::ReturnResult(ERR_UNEXPECTED));

  scoped_refptr<ProxyResolverScriptData> pac_script(
      ProxyResolverScriptData::FromUTF8(kScriptData));
  TestCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_mojo_->SetPacScript(
                                pac_script, callback1.callback()));
  proxy_resolver_mojo_->CancelSetPacScript();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, proxy_resolver_mojo_->SetPacScript(
                                pac_script, callback2.callback()));
  EXPECT_EQ(ERR_UNEXPECTED, callback2.GetResult(ERR_IO_PENDING));
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::ReturnServers(
             GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  SetPacScript(0);

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(OK, request->WaitForResult());

  EXPECT_EQ("DIRECT", request->results().ToPacString());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_WithoutSetPacScript) {
  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->Resolve());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_LoadState) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::SendLoadStateChanged(GURL(kExampleUrl)));
  SetPacScript(0);

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request->load_state());
  while (request->load_state() == LOAD_STATE_RESOLVING_PROXY_FOR_URL)
    base::RunLoop().RunUntilIdle();
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT, request->load_state());
  mojo_proxy_resolver_factory_.GetMockResolver().ClearBlockedClients();
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->WaitForResult());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_MultipleResults) {
  static const char kPacString[] =
      "PROXY foo1:80;DIRECT;SOCKS foo2:1234;"
      "SOCKS5 foo3:1080;HTTPS foo4:443;QUIC foo6:8888";
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::ReturnServers(
             GURL(kExampleUrl), ProxyServersFromPacString(kPacString)));
  SetPacScript(0);

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(OK, request->WaitForResult());

  EXPECT_EQ(kPacString, request->results().ToPacString());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_Error) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::ReturnError(GURL(kExampleUrl), ERR_UNEXPECTED));
  SetPacScript(0);

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(ERR_UNEXPECTED, request->WaitForResult());

  EXPECT_TRUE(request->results().is_empty());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_Cancel) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::WaitForClientDisconnect(GURL(kExampleUrl)));
  SetPacScript(0);

  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  request->Cancel();

  // The Mojo request is still made.
  mojo_proxy_resolver_factory_.GetMockResolver().WaitForNextRequest();
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_MultipleRequests) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::ReturnServers(
             GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::ReturnServers(
             GURL("https://www.chromium.org"),
             ProxyServersFromPacString("HTTPS foo:443")));
  SetPacScript(0);

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
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      1, GetProxyForUrlAction::ReturnServers(
             GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  {
    SetPacScript(0);
    scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->WaitForResult());
    EXPECT_TRUE(request->results().is_empty());
  }

  {
    // Calling GetProxyForURL without first setting the pac script should fail.
    scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->Resolve());
  }

  {
    SetPacScript(1);
    scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
    EXPECT_EQ(OK, request->WaitForResult());
    EXPECT_EQ("DIRECT", request->results().ToPacString());
  }

  EXPECT_EQ(2, mojo_proxy_resolver_factory_.num_create_calls());
}

TEST_F(ProxyResolverMojoTest, GetProxyForURL_ClientClosed) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::DropRequest(GURL(kExampleUrl)));
  SetPacScript(0);

  scoped_ptr<Request> request1(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request1->Resolve());

  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request1->WaitForResult());
  EXPECT_EQ(1, mojo_proxy_resolver_factory_.num_create_calls());
}

TEST_F(ProxyResolverMojoTest, DisconnectAndFailCreate) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));

  {
    SetPacScript(0);
    scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, request->WaitForResult());
    EXPECT_TRUE(request->results().is_empty());
  }

  // The service should attempt to create a new connection, but that should
  // fail.
  {
    scoped_refptr<ProxyResolverScriptData> pac_script(
        ProxyResolverScriptData::FromUTF8(kScriptData));
    TestCompletionCallback callback;
    mojo_proxy_resolver_factory_.FailNextCreate();
    EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED,
              callback.GetResult(proxy_resolver_mojo_->SetPacScript(
                  pac_script, callback.callback())));
  }

  // A third attempt should succeed.
  SetPacScript(2);

  EXPECT_EQ(3, mojo_proxy_resolver_factory_.num_create_calls());
}

TEST_F(ProxyResolverMojoTest, DisconnectAndReconnectInCallback) {
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      0, GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));
  mojo_proxy_resolver_factory_.AddFutureGetProxyAction(
      1, GetProxyForUrlAction::ReturnServers(
             GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));

  // In this first resolve request, the Mojo service is disconnected causing the
  // request to return ERR_PAC_SCRIPT_TERMINATED. In the request callback, form
  // a new connection to a Mojo resolver service by calling SetPacScript().
  SetPacScript(0);
  ProxyInfo results;
  TestCompletionCallback resolve_callback;
  TestCompletionCallback pac_callback;
  ProxyResolver::RequestHandle handle;
  BoundNetLog net_log;
  int error = proxy_resolver_mojo_->GetProxyForURL(
      GURL(kExampleUrl), &results,
      base::Bind(&ProxyResolverMojoTest::RunCallbackAndSetPacScript,
                 base::Unretained(this), resolve_callback.callback(),
                 pac_callback.callback(), 1),
      &handle, net_log);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, resolve_callback.WaitForResult());
  EXPECT_EQ(OK, pac_callback.WaitForResult());

  // Setting the PAC script above should have been successful and let us send a
  // resolve request.
  scoped_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_EQ(ERR_IO_PENDING, request->Resolve());
  EXPECT_EQ(OK, request->WaitForResult());
  EXPECT_EQ("DIRECT", request->results().ToPacString());

  EXPECT_EQ(2, mojo_proxy_resolver_factory_.num_create_calls());
}

}  // namespace net
