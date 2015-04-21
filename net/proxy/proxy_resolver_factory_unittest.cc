// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_factory.h"

#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/mock_proxy_resolver.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_script_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

void Fail(int error) {
  FAIL() << "Unexpected callback called";
}

class TestProxyResolver : public MockAsyncProxyResolver {
 public:
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& script_data,
                   const CompletionCallback& callback) override {
    int result = MockAsyncProxyResolver::SetPacScript(script_data, callback);
    if (synchronous_)
      return error_to_return_;

    return result;
  }

  void set_error_to_return(Error error) {
    synchronous_ = true;
    error_to_return_ = error;
  }

 private:
  bool synchronous_ = false;
  Error error_to_return_ = OK;
};

class TestLegacyProxyResolverFactory : public LegacyProxyResolverFactory {
 public:
  TestLegacyProxyResolverFactory() : LegacyProxyResolverFactory(false) {}

  // LegacyProxyResolverFactory override.
  scoped_ptr<ProxyResolver> CreateProxyResolver() override {
    return make_scoped_ptr(new ForwardingProxyResolver(&resolver_));
  }

  TestProxyResolver& resolver() { return resolver_; }

 private:
  TestProxyResolver resolver_;

  DISALLOW_COPY_AND_ASSIGN(TestLegacyProxyResolverFactory);
};

}  // namespace

class LegacyProxyResolverFactoryTest : public testing::Test {
 public:
  ProxyResolverFactory& factory() { return factory_; }
  TestProxyResolver& mock_resolver() { return factory_.resolver(); }

 private:
  TestLegacyProxyResolverFactory factory_;
};

TEST_F(LegacyProxyResolverFactoryTest, Async_Success) {
  const GURL url("http://proxy");
  TestCompletionCallback callback;
  scoped_ptr<ProxyResolver> resolver;
  scoped_ptr<ProxyResolverFactory::Request> request;
  EXPECT_EQ(ERR_IO_PENDING, factory().CreateProxyResolver(
                                ProxyResolverScriptData::FromURL(url),
                                &resolver, callback.callback(), &request));
  ASSERT_TRUE(mock_resolver().has_pending_set_pac_script_request());
  EXPECT_EQ(
      url,
      mock_resolver().pending_set_pac_script_request()->script_data()->url());
  mock_resolver().pending_set_pac_script_request()->CompleteNow(OK);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(resolver);
}

TEST_F(LegacyProxyResolverFactoryTest, Async_Error) {
  const GURL url("http://proxy");
  TestCompletionCallback callback;
  scoped_ptr<ProxyResolver> resolver;
  scoped_ptr<ProxyResolverFactory::Request> request;
  EXPECT_EQ(ERR_IO_PENDING, factory().CreateProxyResolver(
                                ProxyResolverScriptData::FromURL(url),
                                &resolver, callback.callback(), &request));
  ASSERT_TRUE(mock_resolver().has_pending_set_pac_script_request());
  EXPECT_EQ(
      url,
      mock_resolver().pending_set_pac_script_request()->script_data()->url());
  mock_resolver().pending_set_pac_script_request()->CompleteNow(ERR_FAILED);
  EXPECT_EQ(ERR_FAILED, callback.WaitForResult());
  EXPECT_FALSE(resolver);
}

TEST_F(LegacyProxyResolverFactoryTest, Async_Cancel) {
  const GURL url("http://proxy");
  scoped_ptr<ProxyResolver> resolver;
  scoped_ptr<ProxyResolverFactory::Request> request;
  EXPECT_EQ(ERR_IO_PENDING, factory().CreateProxyResolver(
                                ProxyResolverScriptData::FromURL(url),
                                &resolver, base::Bind(&Fail), &request));
  ASSERT_TRUE(mock_resolver().has_pending_set_pac_script_request());
  EXPECT_EQ(
      url,
      mock_resolver().pending_set_pac_script_request()->script_data()->url());
  request.reset();
  EXPECT_FALSE(resolver);
}

TEST_F(LegacyProxyResolverFactoryTest, Sync_Success) {
  const GURL url("http://proxy");
  TestCompletionCallback callback;
  scoped_ptr<ProxyResolver> resolver;
  scoped_ptr<ProxyResolverFactory::Request> request;
  mock_resolver().set_error_to_return(OK);
  EXPECT_EQ(OK, factory().CreateProxyResolver(
                    ProxyResolverScriptData::FromURL(url), &resolver,
                    callback.callback(), &request));
  ASSERT_TRUE(mock_resolver().has_pending_set_pac_script_request());
  EXPECT_EQ(
      url,
      mock_resolver().pending_set_pac_script_request()->script_data()->url());
  EXPECT_TRUE(resolver);
}

TEST_F(LegacyProxyResolverFactoryTest, Sync_Error) {
  const GURL url("http://proxy");
  TestCompletionCallback callback;
  scoped_ptr<ProxyResolver> resolver;
  scoped_ptr<ProxyResolverFactory::Request> request;
  mock_resolver().set_error_to_return(ERR_FAILED);
  EXPECT_EQ(ERR_FAILED, factory().CreateProxyResolver(
                            ProxyResolverScriptData::FromURL(url), &resolver,
                            callback.callback(), &request));
  ASSERT_TRUE(mock_resolver().has_pending_set_pac_script_request());
  EXPECT_EQ(
      url,
      mock_resolver().pending_set_pac_script_request()->script_data()->url());
  EXPECT_FALSE(resolver);
}

}  // namespace net
