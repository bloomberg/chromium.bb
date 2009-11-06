// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/waitable_event.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/single_threaded_proxy_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// A synchronous mock ProxyResolver implementation, which can be used in
// conjunction with SingleThreadedProxyResolver.
//       - returns a single-item proxy list with the query's host.
class MockProxyResolver : public ProxyResolver {
 public:
  MockProxyResolver()
      : ProxyResolver(true /*expects_pac_bytes*/),
        wrong_loop_(MessageLoop::current()),
        request_count_(0),
        purge_count_(0),
        resolve_latency_ms_(0) {}

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& query_url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             LoadLog* load_log) {
    if (resolve_latency_ms_)
      PlatformThread::Sleep(resolve_latency_ms_);

    CheckIsOnWorkerThread();

    EXPECT_TRUE(callback == NULL);
    EXPECT_TRUE(request == NULL);

    // Write something into |load_log| (doesn't really have any meaning.)
    LoadLog::BeginEvent(load_log, LoadLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE);

    results->UseNamedProxy(query_url.host());

    // Return a success code which represents the request's order.
    return request_count_++;
  }

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

  virtual int SetPacScript(const GURL& pac_url,
                           const std::string& bytes,
                           CompletionCallback* callback) {
    CheckIsOnWorkerThread();
    last_pac_bytes_ = bytes;
    return OK;
  }

  virtual void PurgeMemory() {
    CheckIsOnWorkerThread();
    ++purge_count_;
  }

  int purge_count() const { return purge_count_; }

  const std::string& last_pac_bytes() const { return last_pac_bytes_; }

  void SetResolveLatency(int latency_ms) {
    resolve_latency_ms_ = latency_ms;
  }

 private:
  void CheckIsOnWorkerThread() {
    // We should be running on the worker thread -- while we don't know the
    // message loop of SingleThreadedProxyResolver's worker thread, we do
    // know that it is going to be distinct from the loop running the
    // test, so at least make sure it isn't the main loop.
    EXPECT_NE(MessageLoop::current(), wrong_loop_);
  }

  MessageLoop* wrong_loop_;
  int request_count_;
  int purge_count_;
  std::string last_pac_bytes_;
  int resolve_latency_ms_;
};


// A mock synchronous ProxyResolver which can be set to block upon reaching
// GetProxyForURL().
class BlockableProxyResolver : public MockProxyResolver {
 public:
  BlockableProxyResolver()
      : should_block_(false),
        unblocked_(true, true),
        blocked_(true, false) {
  }

  void Block() {
    should_block_ = true;
    unblocked_.Reset();
  }

  void Unblock() {
    should_block_ = false;
    blocked_.Reset();
    unblocked_.Signal();
  }

  void WaitUntilBlocked() {
    blocked_.Wait();
  }

  virtual int GetProxyForURL(const GURL& query_url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             LoadLog* load_log) {
    if (should_block_) {
      blocked_.Signal();
      unblocked_.Wait();
    }

    return MockProxyResolver::GetProxyForURL(
        query_url, results, callback, request, load_log);
  }

 private:
  bool should_block_;
  base::WaitableEvent unblocked_;
  base::WaitableEvent blocked_;
};

TEST(SingleThreadedProxyResolverTest, Basic) {
  MockProxyResolver* mock = new MockProxyResolver;
  scoped_ptr<SingleThreadedProxyResolver> resolver(
      new SingleThreadedProxyResolver(mock));

  int rv;

  EXPECT_TRUE(resolver->expects_pac_bytes());

  // Call SetPacScriptByData() -- verify that it reaches the synchronous
  // resolver.
  TestCompletionCallback set_script_callback;
  rv = resolver->SetPacScriptByData("pac script bytes", &set_script_callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, set_script_callback.WaitForResult());
  EXPECT_EQ("pac script bytes", mock->last_pac_bytes());

  // Start request 0.
  TestCompletionCallback callback0;
  scoped_refptr<LoadLog> log0(new LoadLog(LoadLog::kUnbounded));
  ProxyInfo results0;
  rv = resolver->GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, NULL, log0);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait for request 0 to finish.
  rv = callback0.WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ("PROXY request0:80", results0.ToPacString());

  // The mock proxy resolver should have written 1 log entry. And
  // on completion, this should have been copied into |log0|.
  EXPECT_EQ(1u, log0->events().size());

  // Start 3 more requests (request1 to request3).

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver->GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver->GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestCompletionCallback callback3;
  ProxyInfo results3;
  rv = resolver->GetProxyForURL(
      GURL("http://request3"), &results3, &callback3, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait for the requests to finish (they must finish in the order they were
  // started, which is what we check for from their magic return value)

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToPacString());

  rv = callback2.WaitForResult();
  EXPECT_EQ(2, rv);
  EXPECT_EQ("PROXY request2:80", results2.ToPacString());

  rv = callback3.WaitForResult();
  EXPECT_EQ(3, rv);
  EXPECT_EQ("PROXY request3:80", results3.ToPacString());

  // Ensure that PurgeMemory() reaches the wrapped resolver and happens on the
  // right thread.
  EXPECT_EQ(0, mock->purge_count());
  resolver->PurgeMemory();
  // There is no way to get a callback directly when PurgeMemory() completes, so
  // we queue up a dummy request after the PurgeMemory() call and wait until it
  // finishes to ensure PurgeMemory() has had a chance to run.
  TestCompletionCallback dummy_callback;
  rv = resolver->SetPacScriptByData("dummy", &dummy_callback);
  EXPECT_EQ(OK, dummy_callback.WaitForResult());
  EXPECT_EQ(1, mock->purge_count());
}

// Cancel a request which is in progress, and then cancel a request which
// is pending.
TEST(SingleThreadedProxyResolverTest, CancelRequest) {
  BlockableProxyResolver* mock = new BlockableProxyResolver;
  scoped_ptr<SingleThreadedProxyResolver> resolver(
      new SingleThreadedProxyResolver(mock));

  int rv;

  // Block the proxy resolver, so no request can complete.
  mock->Block();

  // Start request 0.
  ProxyResolver::RequestHandle request0;
  TestCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver->GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, &request0, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait until requests 0 reaches the worker thread.
  mock->WaitUntilBlocked();

  // Start 3 more requests (request1 : request3).

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver->GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  ProxyResolver::RequestHandle request2;
  TestCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver->GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, &request2, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestCompletionCallback callback3;
  ProxyInfo results3;
  rv = resolver->GetProxyForURL(
      GURL("http://request3"), &results3, &callback3, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel request0 (inprogress) and request2 (pending).
  resolver->CancelRequest(request0);
  resolver->CancelRequest(request2);

  // Unblock the worker thread so the requests can continue running.
  mock->Unblock();

  // Wait for requests 1 and 3 to finish.

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToPacString());

  rv = callback3.WaitForResult();
  // Note that since request2 was cancelled before reaching the resolver,
  // the request count is 2 and not 3 here.
  EXPECT_EQ(2, rv);
  EXPECT_EQ("PROXY request3:80", results3.ToPacString());

  // Requests 0 and 2 which were cancelled, hence their completion callbacks
  // were never summoned.
  EXPECT_FALSE(callback0.have_result());
  EXPECT_FALSE(callback2.have_result());
}

// Test that deleting SingleThreadedProxyResolver while requests are
// outstanding cancels them (and doesn't leak anything).
TEST(SingleThreadedProxyResolverTest, CancelRequestByDeleting) {
  BlockableProxyResolver* mock = new BlockableProxyResolver;
  scoped_ptr<SingleThreadedProxyResolver> resolver(
      new SingleThreadedProxyResolver(mock));

  int rv;

  // Block the proxy resolver, so no request can complete.
  mock->Block();

  // Start 3 requests.

  TestCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver->GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver->GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver->GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait until request 0 reaches the worker thread.
  mock->WaitUntilBlocked();

  // Add some latency, to improve the chance that when
  // SingleThreadedProxyResolver is deleted below we are still running inside
  // of the worker thread. The test will pass regardless, so this race doesn't
  // cause flakiness. However the destruction during execution is a more
  // interesting case to test.
  mock->SetResolveLatency(100);

  // Unblock the worker thread and delete the underlying
  // SingleThreadedProxyResolver immediately.
  mock->Unblock();
  resolver.reset();

  // Give any posted tasks a chance to run (in case there is badness).
  MessageLoop::current()->RunAllPending();

  // Check that none of the outstanding requests were completed.
  EXPECT_FALSE(callback0.have_result());
  EXPECT_FALSE(callback1.have_result());
  EXPECT_FALSE(callback2.have_result());
}

// Cancel an outstanding call to SetPacScriptByData().
TEST(SingleThreadedProxyResolverTest, CancelSetPacScript) {
  BlockableProxyResolver* mock = new BlockableProxyResolver;
  scoped_ptr<SingleThreadedProxyResolver> resolver(
      new SingleThreadedProxyResolver(mock));

  int rv;

  // Block the proxy resolver, so no request can complete.
  mock->Block();

  // Start request 0.
  ProxyResolver::RequestHandle request0;
  TestCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver->GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, &request0, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait until requests 0 reaches the worker thread.
  mock->WaitUntilBlocked();

  TestCompletionCallback set_pac_script_callback;
  rv = resolver->SetPacScriptByData("data", &set_pac_script_callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the SetPacScriptByData request (it can't have finished yet,
  // since the single-thread is currently blocked).
  resolver->CancelSetPacScript();

  // Start 1 more request.

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver->GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Unblock the worker thread so the requests can continue running.
  mock->Unblock();

  // Wait for requests 0 and 1 to finish.

  rv = callback0.WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ("PROXY request0:80", results0.ToPacString());

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToPacString());

  // The SetPacScript callback should never have been completed.
  EXPECT_FALSE(set_pac_script_callback.have_result());
}

}  // namespace
}  // namespace net
