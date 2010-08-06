// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resolve_proxy_msg_helper.h"

#include "net/base/net_errors.h"
#include "net/proxy/mock_proxy_resolver.h"
#include "net/proxy/proxy_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// This ProxyConfigService always returns "http://pac" as the PAC url to use.
class MockProxyConfigService : public net::ProxyConfigService {
 public:
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual bool GetLatestProxyConfig(net::ProxyConfig* results) {
    *results = net::ProxyConfig::CreateFromCustomPacURL(GURL("http://pac"));
    return true;
  }
};

class MyDelegate : public ResolveProxyMsgHelper::Delegate {
 public:
  struct PendingResult {
    PendingResult(IPC::Message* msg,
                  int error_code,
                  const std::string& proxy_list)
        : msg(msg), error_code(error_code), proxy_list(proxy_list) {
    }

    IPC::Message* msg;
    int error_code;
    std::string proxy_list;
  };

  // ResolveProxyMsgHelper::Delegate implementation:
  virtual void OnResolveProxyCompleted(IPC::Message* reply_msg,
                                       int error_code,
                                       const std::string& proxy_list) {
    DCHECK(!pending_result_.get());
    pending_result_.reset(new PendingResult(reply_msg, error_code, proxy_list));
  }

  const PendingResult* pending_result() const { return pending_result_.get(); }

  void clear_pending_result() {
    pending_result_.reset();
  }

 private:
  scoped_ptr<PendingResult> pending_result_;
};

// Issue three sequential requests -- each should succeed.
TEST(ResolveProxyMsgHelperTest, Sequential) {
  net::MockAsyncProxyResolver* resolver = new net::MockAsyncProxyResolver;
  scoped_refptr<net::ProxyService> service(
      new net::ProxyService(new MockProxyConfigService, resolver, NULL));

  MyDelegate delegate;
  ResolveProxyMsgHelper helper(&delegate, service);

  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  scoped_ptr<IPC::Message> msg1(new IPC::Message());
  scoped_ptr<IPC::Message> msg2(new IPC::Message());
  scoped_ptr<IPC::Message> msg3(new IPC::Message());

  // Execute each request sequentially (so there are never 2 requests
  // outstanding at the same time).

  helper.Start(url1, msg1.get());

  // Finish ProxyService's initialization.
  resolver->pending_set_pac_script_request()->CompleteNow(net::OK);

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url1, resolver->pending_requests()[0]->url());
  resolver->pending_requests()[0]->results()->UseNamedProxy("result1:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(msg1.get(), delegate.pending_result()->msg);
  EXPECT_EQ(net::OK, delegate.pending_result()->error_code);
  EXPECT_EQ("PROXY result1:80", delegate.pending_result()->proxy_list);
  delegate.clear_pending_result();

  helper.Start(url2, msg2.get());

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url2, resolver->pending_requests()[0]->url());
  resolver->pending_requests()[0]->results()->UseNamedProxy("result2:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(msg2.get(), delegate.pending_result()->msg);
  EXPECT_EQ(net::OK, delegate.pending_result()->error_code);
  EXPECT_EQ("PROXY result2:80", delegate.pending_result()->proxy_list);
  delegate.clear_pending_result();

  helper.Start(url3, msg3.get());

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url3, resolver->pending_requests()[0]->url());
  resolver->pending_requests()[0]->results()->UseNamedProxy("result3:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(msg3.get(), delegate.pending_result()->msg);
  EXPECT_EQ(net::OK, delegate.pending_result()->error_code);
  EXPECT_EQ("PROXY result3:80", delegate.pending_result()->proxy_list);
  delegate.clear_pending_result();
}

// Issue a request while one is already in progress -- should be queued.
TEST(ResolveProxyMsgHelperTest, QueueRequests) {
  net::MockAsyncProxyResolver* resolver = new net::MockAsyncProxyResolver;
  scoped_refptr<net::ProxyService> service(
      new net::ProxyService(new MockProxyConfigService, resolver, NULL));

  MyDelegate delegate;
  ResolveProxyMsgHelper helper(&delegate, service);

  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  scoped_ptr<IPC::Message> msg1(new IPC::Message());
  scoped_ptr<IPC::Message> msg2(new IPC::Message());
  scoped_ptr<IPC::Message> msg3(new IPC::Message());

  // Start three requests. Since the proxy resolver is async, all the
  // requests will be pending.

  helper.Start(url1, msg1.get());

  // Finish ProxyService's initialization.
  resolver->pending_set_pac_script_request()->CompleteNow(net::OK);

  helper.Start(url2, msg2.get());
  helper.Start(url3, msg3.get());

  // ResolveProxyHelper only keeps 1 request outstanding in ProxyService
  // at a time.
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url1, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy("result1:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(msg1.get(), delegate.pending_result()->msg);
  EXPECT_EQ(net::OK, delegate.pending_result()->error_code);
  EXPECT_EQ("PROXY result1:80", delegate.pending_result()->proxy_list);
  delegate.clear_pending_result();

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url2, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy("result2:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(msg2.get(), delegate.pending_result()->msg);
  EXPECT_EQ(net::OK, delegate.pending_result()->error_code);
  EXPECT_EQ("PROXY result2:80", delegate.pending_result()->proxy_list);
  delegate.clear_pending_result();

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url3, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy("result3:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(msg3.get(), delegate.pending_result()->msg);
  EXPECT_EQ(net::OK, delegate.pending_result()->error_code);
  EXPECT_EQ("PROXY result3:80", delegate.pending_result()->proxy_list);
  delegate.clear_pending_result();
}

// Delete the helper while a request is in progress, and others are pending.
TEST(ResolveProxyMsgHelperTest, CancelPendingRequests) {
  net::MockAsyncProxyResolver* resolver = new net::MockAsyncProxyResolver;
  scoped_refptr<net::ProxyService> service(
      new net::ProxyService(new MockProxyConfigService, resolver, NULL));

  MyDelegate delegate;
  scoped_ptr<ResolveProxyMsgHelper> helper(
      new ResolveProxyMsgHelper(&delegate, service));

  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // NOTE: these are not scoped ptr, since they will be deleted by the
  // request's cancellation.
  IPC::Message* msg1 = new IPC::Message();
  IPC::Message* msg2 = new IPC::Message();
  IPC::Message* msg3 = new IPC::Message();

  // Start three requests. Since the proxy resolver is async, all the
  // requests will be pending.

  helper->Start(url1, msg1);

  // Finish ProxyService's initialization.
  resolver->pending_set_pac_script_request()->CompleteNow(net::OK);

  helper->Start(url2, msg2);
  helper->Start(url3, msg3);

  // ResolveProxyHelper only keeps 1 request outstanding in ProxyService
  // at a time.
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url1, resolver->pending_requests()[0]->url());

  // Delete the underlying ResolveProxyMsgHelper -- this should cancel all
  // the requests which are outstanding.
  helper.reset();

  // The pending requests sent to the proxy resolver should have been cancelled.

  EXPECT_EQ(0u, resolver->pending_requests().size());

  EXPECT_TRUE(delegate.pending_result() == NULL);

  // It should also be the case that msg1, msg2, msg3 were deleted by the
  // cancellation. (Else will show up as a leak in Purify/Valgrind).
}
