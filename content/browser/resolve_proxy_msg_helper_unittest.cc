// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resolve_proxy_msg_helper.h"

#include <tuple>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/common/view_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestResolveProxyMsgHelper : public ResolveProxyMsgHelper {
 public:
  explicit TestResolveProxyMsgHelper(IPC::Listener* listener)
      : ResolveProxyMsgHelper(0 /* renderer_process_host_id */),
        listener_(listener) {}

  bool Send(IPC::Message* message) override {
    listener_->OnMessageReceived(*message);
    delete message;
    return true;
  }

  bool SendRequestToNetworkService(
      const GURL& url,
      network::mojom::ProxyLookupClientPtr proxy_lookup_client) override {
    // Only one request should be send at a time.
    EXPECT_FALSE(proxy_lookup_client_);

    if (fail_to_send_request_)
      return false;

    pending_url_ = url;
    proxy_lookup_client_ = std::move(proxy_lookup_client);
    return true;
  }

  network::mojom::ProxyLookupClientPtr ClaimProxyLookupClient() {
    EXPECT_TRUE(proxy_lookup_client_);
    return std::move(proxy_lookup_client_);
  }

  const GURL& pending_url() const { return pending_url_; }

  void set_fail_to_send_request(bool fail_to_send_request) {
    fail_to_send_request_ = fail_to_send_request;
  }

 protected:
  ~TestResolveProxyMsgHelper() override {}

  IPC::Listener* listener_;

  bool fail_to_send_request_ = false;

  network::mojom::ProxyLookupClientPtr proxy_lookup_client_;
  GURL pending_url_;

  DISALLOW_COPY_AND_ASSIGN(TestResolveProxyMsgHelper);
};

class ResolveProxyMsgHelperTest : public testing::Test, public IPC::Listener {
 public:
  struct PendingResult {
    PendingResult(bool result,
                  const std::string& proxy_list)
        : result(result), proxy_list(proxy_list) {
    }

    bool result;
    std::string proxy_list;
  };

  ResolveProxyMsgHelperTest()
      : helper_(base::MakeRefCounted<TestResolveProxyMsgHelper>(this)) {
    test_sink_.AddFilter(this);
  }

 protected:
  const PendingResult* pending_result() const { return pending_result_.get(); }

  void clear_pending_result() {
    pending_result_.reset();
  }

  IPC::Message* GenerateReply() {
    bool temp_bool;
    std::string temp_string;
    ViewHostMsg_ResolveProxy message(GURL(), &temp_bool, &temp_string);
    return IPC::SyncMessage::GenerateReply(&message);
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    ViewHostMsg_ResolveProxy::ReplyParam reply_data;
    EXPECT_TRUE(ViewHostMsg_ResolveProxy::ReadReplyParam(&msg, &reply_data));
    DCHECK(!pending_result_.get());
    pending_result_.reset(
        new PendingResult(std::get<0>(reply_data), std::get<1>(reply_data)));
    test_sink_.ClearMessages();
    return true;
  }

  TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<TestResolveProxyMsgHelper> helper_;
  std::unique_ptr<PendingResult> pending_result_;

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

  // There should be a pending proxy lookup request. Respond to it.
  EXPECT_EQ(url1, helper_->pending_url());
  network::mojom::ProxyLookupClientPtr proxy_lookup_client =
      helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("result1:80");
  proxy_lookup_client->OnProxyLookupComplete(proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(true, pending_result()->result);
  EXPECT_EQ("PROXY result1:80", pending_result()->proxy_list);
  clear_pending_result();

  helper_->OnResolveProxy(url2, msg2);

  EXPECT_EQ(url2, helper_->pending_url());
  proxy_lookup_client = helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  proxy_info.UseNamedProxy("result2:80");
  proxy_lookup_client->OnProxyLookupComplete(proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(true, pending_result()->result);
  EXPECT_EQ("PROXY result2:80", pending_result()->proxy_list);
  clear_pending_result();

  helper_->OnResolveProxy(url3, msg3);

  EXPECT_EQ(url3, helper_->pending_url());
  proxy_lookup_client = helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  proxy_info.UseNamedProxy("result3:80");
  proxy_lookup_client->OnProxyLookupComplete(proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(true, pending_result()->result);
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

  // Start three requests. All the requests will be pending.

  helper_->OnResolveProxy(url1, msg1);
  helper_->OnResolveProxy(url2, msg2);
  helper_->OnResolveProxy(url3, msg3);

  // Complete first request.
  EXPECT_EQ(url1, helper_->pending_url());
  network::mojom::ProxyLookupClientPtr proxy_lookup_client =
      helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("result1:80");
  proxy_lookup_client->OnProxyLookupComplete(proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(true, pending_result()->result);
  EXPECT_EQ("PROXY result1:80", pending_result()->proxy_list);
  clear_pending_result();

  // Complete second request.
  EXPECT_EQ(url2, helper_->pending_url());
  proxy_lookup_client = helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  proxy_info.UseNamedProxy("result2:80");
  proxy_lookup_client->OnProxyLookupComplete(proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(true, pending_result()->result);
  EXPECT_EQ("PROXY result2:80", pending_result()->proxy_list);
  clear_pending_result();

  // Complete third request.
  EXPECT_EQ(url3, helper_->pending_url());
  proxy_lookup_client = helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  proxy_info.UseNamedProxy("result3:80");
  proxy_lookup_client->OnProxyLookupComplete(proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(true, pending_result()->result);
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
  helper_->OnResolveProxy(url2, msg2);
  helper_->OnResolveProxy(url3, msg3);

  // Check the first request is pending.
  EXPECT_EQ(url1, helper_->pending_url());
  network::mojom::ProxyLookupClientPtr proxy_lookup_client =
      helper_->ClaimProxyLookupClient();
  base::RunLoop run_loop;
  proxy_lookup_client.set_connection_error_handler(run_loop.QuitClosure());

  // Delete the underlying ResolveProxyMsgHelper -- this should cancel all
  // the requests which are outstanding.
  helper_ = nullptr;

  // The pending request sent to the proxy resolver should have been cancelled.
  run_loop.Run();

  EXPECT_TRUE(pending_result() == nullptr);

  // It should also be the case that msg1, msg2, msg3 were deleted by the
  // cancellation. (Else will show up as a leak).
}

// Issue a request that fails.
TEST_F(ResolveProxyMsgHelperTest, RequestFails) {
  GURL url("http://www.google.com/");

  // Message will be deleted by the sink.
  IPC::Message* msg = GenerateReply();

  helper_->OnResolveProxy(url, msg);

  // There should be a pending proxy lookup request. Respond to it.
  EXPECT_EQ(url, helper_->pending_url());
  network::mojom::ProxyLookupClientPtr proxy_lookup_client =
      helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  proxy_lookup_client->OnProxyLookupComplete(base::nullopt);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(false, pending_result()->result);
  EXPECT_EQ("", pending_result()->proxy_list);
  clear_pending_result();
}

// Issue a request, only to have the Mojo pipe closed.
TEST_F(ResolveProxyMsgHelperTest, PipeClosed) {
  GURL url("http://www.google.com/");

  // Message will be deleted by the sink.
  IPC::Message* msg = GenerateReply();

  helper_->OnResolveProxy(url, msg);

  // There should be a pending proxy lookup request. Respond to it by closing
  // the pipe.
  EXPECT_EQ(url, helper_->pending_url());
  network::mojom::ProxyLookupClientPtr proxy_lookup_client =
      helper_->ClaimProxyLookupClient();
  ASSERT_TRUE(proxy_lookup_client);
  proxy_lookup_client.reset();
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_EQ(false, pending_result()->result);
  EXPECT_EQ("", pending_result()->proxy_list);
  clear_pending_result();
}

// Fail to send a request to the network service.
TEST_F(ResolveProxyMsgHelperTest, FailToSendRequest) {
  GURL url("http://www.google.com/");

  // Message will be deleted by the sink.
  IPC::Message* msg = GenerateReply();

  helper_->set_fail_to_send_request(true);

  helper_->OnResolveProxy(url, msg);
  // No request should be pending.
  EXPECT_TRUE(helper_->pending_url().is_empty());

  // Check result.
  EXPECT_EQ(false, pending_result()->result);
  EXPECT_EQ("", pending_result()->proxy_list);
  clear_pending_result();
}

}  // namespace content
