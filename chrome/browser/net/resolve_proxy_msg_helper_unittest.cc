// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resolve_proxy_msg_helper.h"

#include "content/common/child_process_messages.h"
#include "ipc/ipc_test_sink.h"
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

class ResolveProxyMsgHelperTest : public testing::Test,
                                  public IPC::Channel::Listener {
 public:
  struct PendingResult {
    PendingResult(int error_code,
                  const std::string& proxy_list)
        : error_code(error_code), proxy_list(proxy_list) {
    }

    int error_code;
    std::string proxy_list;
  };

  ResolveProxyMsgHelperTest()
      : resolver_(new net::MockAsyncProxyResolver),
        service_(new net::ProxyService(
            new MockProxyConfigService, resolver_, NULL)),
        helper_(new ResolveProxyMsgHelper(service_.get())),
        message_loop_(MessageLoop::TYPE_IO),
        io_thread_(BrowserThread::IO, &message_loop_) {
    test_sink_.AddFilter(this);
    helper_->OnFilterAdded(&test_sink_);
  }

 protected:
  const PendingResult* pending_result() const { return pending_result_.get(); }

  void clear_pending_result() {
    pending_result_.reset();
  }

  IPC::Message* GenerateReply() {
    int temp_int;
    std::string temp_string;
    ChildProcessHostMsg_ResolveProxy message(GURL(), &temp_int, &temp_string);
    return IPC::SyncMessage::GenerateReply(&message);
  }

  net::MockAsyncProxyResolver* resolver_;
  scoped_refptr<net::ProxyService> service_;
  scoped_refptr<ResolveProxyMsgHelper> helper_;
  scoped_ptr<PendingResult> pending_result_;

 private:
  virtual bool OnMessageReceived(const IPC::Message& msg) {
    TupleTypes<ChildProcessHostMsg_ResolveProxy::ReplyParam>::ValueTuple
        reply_data;
    EXPECT_TRUE(
        ChildProcessHostMsg_ResolveProxy::ReadReplyParam(&msg, &reply_data));
    DCHECK(!pending_result_.get());
    pending_result_.reset(new PendingResult(reply_data.a, reply_data.b));
    test_sink_.ClearMessages();
    return true;
  }

  MessageLoop message_loop_;
  BrowserThread io_thread_;
  IPC::TestSink test_sink_;
};

// Issue three sequential requests -- each should succeed.
TEST_F(ResolveProxyMsgHelperTest, Sequential) {
  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // Messages are deleted by the sink.
  IPC::Message* msg1 = GenerateReply();
  IPC::Message* msg2 = GenerateReply();
  IPC::Message* msg3 = GenerateReply();

  // Execute each request sequentially (so there are never 2 requests
  // outstanding at the same time).

  helper_->OnResolveProxy(url1, msg1);

  // Finish ProxyService's initialization.
  resolver_->pending_set_pac_script_request()->CompleteNow(net::OK);

  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url1, resolver_->pending_requests()[0]->url());
  resolver_->pending_requests()[0]->results()->UseNamedProxy("result1:80");
  resolver_->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(net::OK, pending_result()->error_code);
  EXPECT_EQ("PROXY result1:80", pending_result()->proxy_list);
  clear_pending_result();

  helper_->OnResolveProxy(url2, msg2);

  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url2, resolver_->pending_requests()[0]->url());
  resolver_->pending_requests()[0]->results()->UseNamedProxy("result2:80");
  resolver_->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(net::OK, pending_result()->error_code);
  EXPECT_EQ("PROXY result2:80", pending_result()->proxy_list);
  clear_pending_result();

  helper_->OnResolveProxy(url3, msg3);

  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url3, resolver_->pending_requests()[0]->url());
  resolver_->pending_requests()[0]->results()->UseNamedProxy("result3:80");
  resolver_->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(net::OK, pending_result()->error_code);
  EXPECT_EQ("PROXY result3:80", pending_result()->proxy_list);
  clear_pending_result();
}

// Issue a request while one is already in progress -- should be queued.
TEST_F(ResolveProxyMsgHelperTest, QueueRequests) {
  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  IPC::Message* msg1 = GenerateReply();
  IPC::Message* msg2 = GenerateReply();
  IPC::Message* msg3 = GenerateReply();

  // Start three requests. Since the proxy resolver is async, all the
  // requests will be pending.

  helper_->OnResolveProxy(url1, msg1);

  // Finish ProxyService's initialization.
  resolver_->pending_set_pac_script_request()->CompleteNow(net::OK);

  helper_->OnResolveProxy(url2, msg2);
  helper_->OnResolveProxy(url3, msg3);

  // ResolveProxyHelper only keeps 1 request outstanding in ProxyService
  // at a time.
  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url1, resolver_->pending_requests()[0]->url());

  resolver_->pending_requests()[0]->results()->UseNamedProxy("result1:80");
  resolver_->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(net::OK, pending_result()->error_code);
  EXPECT_EQ("PROXY result1:80", pending_result()->proxy_list);
  clear_pending_result();

  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url2, resolver_->pending_requests()[0]->url());

  resolver_->pending_requests()[0]->results()->UseNamedProxy("result2:80");
  resolver_->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(net::OK, pending_result()->error_code);
  EXPECT_EQ("PROXY result2:80", pending_result()->proxy_list);
  clear_pending_result();

  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url3, resolver_->pending_requests()[0]->url());

  resolver_->pending_requests()[0]->results()->UseNamedProxy("result3:80");
  resolver_->pending_requests()[0]->CompleteNow(net::OK);

  // Check result.
  EXPECT_EQ(net::OK, pending_result()->error_code);
  EXPECT_EQ("PROXY result3:80", pending_result()->proxy_list);
  clear_pending_result();
}

// Delete the helper while a request is in progress, and others are pending.
TEST_F(ResolveProxyMsgHelperTest, CancelPendingRequests) {
  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // They will be deleted by the request's cancellation.
  IPC::Message* msg1 = GenerateReply();
  IPC::Message* msg2 = GenerateReply();
  IPC::Message* msg3 = GenerateReply();

  // Start three requests. Since the proxy resolver is async, all the
  // requests will be pending.

  helper_->OnResolveProxy(url1, msg1);

  // Finish ProxyService's initialization.
  resolver_->pending_set_pac_script_request()->CompleteNow(net::OK);

  helper_->OnResolveProxy(url2, msg2);
  helper_->OnResolveProxy(url3, msg3);

  // ResolveProxyHelper only keeps 1 request outstanding in ProxyService
  // at a time.
  ASSERT_EQ(1u, resolver_->pending_requests().size());
  EXPECT_EQ(url1, resolver_->pending_requests()[0]->url());

  // Delete the underlying ResolveProxyMsgHelper -- this should cancel all
  // the requests which are outstanding.
  helper_ = NULL;

  // The pending requests sent to the proxy resolver should have been cancelled.

  EXPECT_EQ(0u, resolver_->pending_requests().size());

  EXPECT_TRUE(pending_result() == NULL);

  // It should also be the case that msg1, msg2, msg3 were deleted by the
  // cancellation. (Else will show up as a leak in Purify/Valgrind).
}
