// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_factory_impl.h"

#include "net/proxy/mock_proxy_resolver.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
namespace {

class FakeProxyResolver : public MockAsyncProxyResolverExpectsBytes {
 public:
  explicit FakeProxyResolver(const base::Closure& on_destruction)
      : on_destruction_(on_destruction) {}

  ~FakeProxyResolver() override { on_destruction_.Run(); }

 private:
  const base::Closure on_destruction_;
};

}  // namespace

class MojoProxyResolverFactoryImplTest : public testing::Test,
                                         public mojo::ErrorHandler {
 protected:
  enum Event {
    NONE,
    RESOLVER_CREATED,
    CONNECTION_ERROR,
    RESOLVER_DESTROYED,
  };

  void SetUp() override {
    new MojoProxyResolverFactoryImpl(
        base::Bind(&MojoProxyResolverFactoryImplTest::CreateFakeProxyResolver,
                   base::Unretained(this)),
        mojo::GetProxy(&factory_));
  }

  void OnConnectionError() override { waiter_.NotifyEvent(CONNECTION_ERROR); }

  scoped_ptr<ProxyResolver> CreateFakeProxyResolver(
      HostResolver* host_resolver,
      const ProxyResolver::LoadStateChangedCallback& callback) {
    EXPECT_TRUE(host_resolver);
    instances_created_++;
    waiter_.NotifyEvent(RESOLVER_CREATED);
    return make_scoped_ptr(new FakeProxyResolver(base::Bind(
        &MojoProxyResolverFactoryImplTest::OnFakeProxyInstanceDestroyed,
        base::Unretained(this))));
  }

  void OnFakeProxyInstanceDestroyed() {
    instances_destroyed_++;
    waiter_.NotifyEvent(RESOLVER_DESTROYED);
  }

  interfaces::ProxyResolverFactoryPtr factory_;

  int instances_created_ = 0;
  int instances_destroyed_ = 0;

  EventWaiter<Event> waiter_;
};

TEST_F(MojoProxyResolverFactoryImplTest, DisconnectHostResolver) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  factory_->CreateResolver(mojo::GetProxy(&proxy_resolver),
                           host_resolver.Pass());
  proxy_resolver.set_error_handler(this);
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(1, instances_created_);
  EXPECT_EQ(0, instances_destroyed_);
  host_resolver_request = mojo::InterfaceRequest<interfaces::HostResolver>();
  waiter_.WaitForEvent(CONNECTION_ERROR);
  EXPECT_EQ(1, instances_created_);
  EXPECT_EQ(1, instances_destroyed_);
}

TEST_F(MojoProxyResolverFactoryImplTest, DisconnectProxyResolverClient) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  mojo::Binding<interfaces::HostResolver> binding(nullptr, &host_resolver);
  binding.set_error_handler(this);
  factory_->CreateResolver(mojo::GetProxy(&proxy_resolver),
                           host_resolver.Pass());
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(1, instances_created_);
  EXPECT_EQ(0, instances_destroyed_);
  proxy_resolver.reset();
  waiter_.WaitForEvent(CONNECTION_ERROR);
  EXPECT_EQ(1, instances_created_);
  EXPECT_EQ(1, instances_destroyed_);
}

TEST_F(MojoProxyResolverFactoryImplTest, DisconnectBoth) {
  interfaces::ProxyResolverPtr proxy_resolver;
  interfaces::HostResolverPtr host_resolver;
  mojo::InterfaceRequest<interfaces::HostResolver> host_resolver_request =
      mojo::GetProxy(&host_resolver);
  factory_->CreateResolver(mojo::GetProxy(&proxy_resolver),
                           host_resolver.Pass());
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(1, instances_created_);
  EXPECT_EQ(0, instances_destroyed_);
  proxy_resolver.reset();
  host_resolver_request = mojo::InterfaceRequest<interfaces::HostResolver>();
  waiter_.WaitForEvent(RESOLVER_DESTROYED);
  EXPECT_EQ(1, instances_created_);
  EXPECT_EQ(1, instances_destroyed_);
}

}  // namespace net
