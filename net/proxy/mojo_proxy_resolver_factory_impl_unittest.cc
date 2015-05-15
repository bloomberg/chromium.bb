// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_factory_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/mock_proxy_resolver.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
namespace {

const char kScriptData[] = "FooBarBaz";

class FakeProxyResolver : public MockAsyncProxyResolverExpectsBytes {
 public:
  explicit FakeProxyResolver(const base::Closure& on_destruction)
      : on_destruction_(on_destruction) {}

  ~FakeProxyResolver() override { on_destruction_.Run(); }

 private:
  const base::Closure on_destruction_;
};

enum Event {
  NONE,
  RESOLVER_CREATED,
  CONNECTION_ERROR,
  RESOLVER_DESTROYED,
};

class TestProxyResolverFactory : public MockAsyncProxyResolverFactory {
 public:
  explicit TestProxyResolverFactory(EventWaiter<Event>* waiter)
      : MockAsyncProxyResolverFactory(true), waiter_(waiter) {}

  int CreateProxyResolver(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver>* resolver,
      const CompletionCallback& callback,
      scoped_ptr<ProxyResolverFactory::Request>* request) override {
    waiter_->NotifyEvent(RESOLVER_CREATED);
    return MockAsyncProxyResolverFactory::CreateProxyResolver(
        pac_script, resolver, callback, request);
  }

 private:
  EventWaiter<Event>* waiter_;
};

}  // namespace

class MojoProxyResolverFactoryImplTest
    : public testing::Test,
      public mojo::ErrorHandler,
      public interfaces::ProxyResolverFactoryRequestClient {
 public:
  void SetUp() override {
    new MojoProxyResolverFactoryImpl(
        base::Bind(
            &MojoProxyResolverFactoryImplTest::CreateFakeProxyResolverFactory,
            base::Unretained(this)),
        mojo::GetProxy(&factory_));
    mock_factory_owner_.reset(new TestProxyResolverFactory(&waiter_));
    mock_factory_ = mock_factory_owner_.get();
  }

  void OnConnectionError() override { waiter_.NotifyEvent(CONNECTION_ERROR); }

  scoped_ptr<ProxyResolverFactory> CreateFakeProxyResolverFactory(
      HostResolver* host_resolver,
      scoped_ptr<ProxyResolverErrorObserver> error_observer,
      const ProxyResolver::LoadStateChangedCallback& callback) {
    EXPECT_TRUE(host_resolver);
    EXPECT_FALSE(callback.is_null());
    DCHECK(mock_factory_owner_);
    return mock_factory_owner_.Pass();
  }

  void OnFakeProxyInstanceDestroyed() {
    instances_destroyed_++;
    waiter_.NotifyEvent(RESOLVER_DESTROYED);
  }

  void ReportResult(int32_t error) override { create_callback_.Run(error); }

 protected:
  scoped_ptr<TestProxyResolverFactory> mock_factory_owner_;
  TestProxyResolverFactory* mock_factory_;
  interfaces::ProxyResolverFactoryPtr factory_;

  int instances_destroyed_ = 0;
  CompletionCallback create_callback_;

  EventWaiter<Event> waiter_;
};

TEST_F(MojoProxyResolverFactoryImplTest, DisconnectHostResolver) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  mock_factory_->pending_requests()[0]->CompleteNow(
      OK, make_scoped_ptr(new FakeProxyResolver(base::Bind(
              &MojoProxyResolverFactoryImplTest::OnFakeProxyInstanceDestroyed,
              base::Unretained(this)))));
  EXPECT_EQ(OK, create_callback.WaitForResult());
  host_resolver_request = mojo::InterfaceRequest<interfaces::HostResolver>();
  waiter_.WaitForEvent(CONNECTION_ERROR);
  EXPECT_EQ(1, instances_destroyed_);
}

TEST_F(MojoProxyResolverFactoryImplTest, DisconnectProxyResolverClient) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  mojo::Binding<interfaces::HostResolver> binding(nullptr, &host_resolver);
  binding.set_error_handler(this);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  mock_factory_->pending_requests()[0]->CompleteNow(
      OK, make_scoped_ptr(new FakeProxyResolver(base::Bind(
              &MojoProxyResolverFactoryImplTest::OnFakeProxyInstanceDestroyed,
              base::Unretained(this)))));
  EXPECT_EQ(OK, create_callback.WaitForResult());
  proxy_resolver.reset();
  waiter_.WaitForEvent(CONNECTION_ERROR);
  EXPECT_EQ(1, instances_destroyed_);
}

TEST_F(MojoProxyResolverFactoryImplTest, DisconnectBoth) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  mock_factory_->pending_requests()[0]->CompleteNow(
      OK, make_scoped_ptr(new FakeProxyResolver(base::Bind(
              &MojoProxyResolverFactoryImplTest::OnFakeProxyInstanceDestroyed,
              base::Unretained(this)))));
  EXPECT_EQ(OK, create_callback.WaitForResult());
  proxy_resolver.reset();
  host_resolver_request = mojo::InterfaceRequest<interfaces::HostResolver>();
  waiter_.WaitForEvent(RESOLVER_DESTROYED);
  EXPECT_EQ(1, instances_destroyed_);
}

TEST_F(MojoProxyResolverFactoryImplTest, Error) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  mock_factory_->pending_requests()[0]->CompleteNow(ERR_PAC_SCRIPT_FAILED,
                                                    nullptr);
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, create_callback.WaitForResult());
}

TEST_F(MojoProxyResolverFactoryImplTest,
       DisconnectHostResolverDuringResolveCreation) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  host_resolver_request = mojo::InterfaceRequest<interfaces::HostResolver>();
  TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  waiter_.WaitForEvent(CONNECTION_ERROR);
  EXPECT_EQ(ERR_PAC_SCRIPT_TERMINATED, create_callback.WaitForResult());
}

TEST_F(MojoProxyResolverFactoryImplTest,
       DisconnectClientDuringResolverCreation) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  mojo::Binding<interfaces::HostResolver> binding(nullptr, &host_resolver);
  binding.set_error_handler(this);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  client_binding.Close();
  waiter_.WaitForEvent(CONNECTION_ERROR);
}

TEST_F(MojoProxyResolverFactoryImplTest,
       DisconnectFactoryDuringResolverCreation) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  mojo::Binding<interfaces::HostResolver> binding(nullptr, &host_resolver);
  binding.set_error_handler(this);
  interfaces::ProxyResolverErrorObserverPtr error_observer;
  mojo::GetProxy(&error_observer);
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr;
  mojo::Binding<ProxyResolverFactoryRequestClient> client_binding(
      this, mojo::GetProxy(&client_ptr));
  factory_->CreateResolver(
      mojo::String::From(kScriptData), mojo::GetProxy(&proxy_resolver),
      host_resolver.Pass(), error_observer.Pass(), client_ptr.Pass());
  proxy_resolver.set_error_handler(this);
  client_binding.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->pending_requests().size());
  EXPECT_EQ(base::ASCIIToUTF16(kScriptData),
            mock_factory_->pending_requests()[0]->script_data()->utf16());
  factory_.reset();
  waiter_.WaitForEvent(CONNECTION_ERROR);
}

}  // namespace net
