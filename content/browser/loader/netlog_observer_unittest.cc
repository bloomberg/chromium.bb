// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/request_priority.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/quic/core/quic_types.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_netlog_params.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kDefaultURL[] = "https://example.test";

std::unique_ptr<base::Value> QuicRequestCallback(
    net::QuicStreamId stream_id,
    const net::SpdyHeaderBlock* headers,
    net::SpdyPriority priority,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(
          SpdyHeaderBlockNetLogCallback(headers, capture_mode).release()));
  return std::move(dict);
}

}  // namespace

class NetLogObserverTest : public testing::Test {
 public:
  NetLogObserverTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP), context_(true) {}

  ~NetLogObserverTest() override {}

  void SetUp() override {
    context_.Init();
    context_.set_net_log(&net_log_);
    NetLogObserver::Attach(context_.net_log());
    request_ = context_.CreateRequest(GURL(kDefaultURL), net::DEFAULT_PRIORITY,
                                      nullptr);
    resource_context_ = base::MakeUnique<MockResourceContext>(&context_);
    requester_info_ = ResourceRequesterInfo::CreateForRendererTesting(1);
    ResourceRequestInfoImpl* info = new ResourceRequestInfoImpl(
        requester_info_,
        0,                                     // route_id
        -1,                                    // frame_tree_node_id
        0,                                     // origin_pid
        0,                                     // request_id
        0,                                     // render_frame_id
        false,                                 // is_main_frame
        false,                                 // parent_is_main_frame
        RESOURCE_TYPE_IMAGE,                   // resource_type
        ui::PAGE_TRANSITION_LINK,              // transition_type
        false,                                 // should_replace_current_entry
        false,                                 // is_download
        false,                                 // is_stream
        false,                                 // allow_download
        false,                                 // has_user_gesture
        false,                                 // enable load timing
        false,                                 // enable upload progress
        false,                                 // do_not_prompt_for_login
        blink::WebReferrerPolicyDefault,       // referrer_policy
        blink::WebPageVisibilityStateVisible,  // visibility_state
        resource_context_.get(),               // context
        true,                                  // report_raw_headers
        true,                                  // is_async
        PREVIEWS_OFF,                          // previews_state
        std::string(),                         // original_headers
        nullptr,                               // body
        false);                                // initiated_in_secure_context
    info->AssociateWithRequest(request_.get());
    std::string method = "GET";
    GURL url(kDefaultURL);
    request_->net_log().BeginEvent(
        net::NetLogEventType::URL_REQUEST_START_JOB,
        base::Bind(&net::NetLogURLRequestStartCallback, &url, &method, 0, -1));
  }

  void TearDown() override { NetLogObserver::Detach(); }

  void VerifyHeaderValue(const std::string& header,
                         const std::string& expected_value) {
    scoped_refptr<ResourceResponse> response(new ResourceResponse);

    NetLogObserver::PopulateResponseInfo(request_.get(), response.get());

    int header_was_in_list = false;
    for (auto header_pair : response->head.devtools_info->request_headers) {
      if (header == header_pair.first) {
        EXPECT_EQ(expected_value, header_pair.second);
        header_was_in_list = true;
      }
    }
    EXPECT_TRUE(header_was_in_list);
  }

 protected:
  net::NetLog net_log_;
  TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;
  std::unique_ptr<MockResourceContext> resource_context_;
  scoped_refptr<ResourceRequesterInfo> requester_info_;
  std::unique_ptr<net::URLRequest> request_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetLogObserverTest);
};

// Verify that the response info is populated with headers for HTTP jobs.
TEST_F(NetLogObserverTest, TestHTTPEntry) {
  std::string header("header");
  std::string value("value");
  std::string request_line("HTTP request line");

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(header, value);

  request_->net_log().AddEvent(
      net::NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
      base::Bind(&net::HttpRequestHeaders::NetLogCallback,
                 base::Unretained(&request_headers), &request_line));

  VerifyHeaderValue(header, value);
}

// Verify that the response info is populated with headers for HTTP2 jobs.
TEST_F(NetLogObserverTest, TestHTTP2Entry) {
  std::string header("header");
  std::string value("value");
  net::SpdyHeaderBlock request_headers;

  request_headers[header] = value;
  request_->net_log().AddEvent(
      net::NetLogEventType::HTTP_TRANSACTION_HTTP2_SEND_REQUEST_HEADERS,
      base::Bind(&net::SpdyHeaderBlockNetLogCallback, &request_headers));

  VerifyHeaderValue(header, value);
}

// Verify that the response info is populated with headers for QUIC jobs.
TEST_F(NetLogObserverTest, TestQUICEntry) {
  std::string header("header");
  std::string value("value");
  net::SpdyHeaderBlock request_headers;

  request_headers[header] = value;
  request_->net_log().AddEvent(
      net::NetLogEventType::HTTP_TRANSACTION_QUIC_SEND_REQUEST_HEADERS,
      base::Bind(&QuicRequestCallback, 1, &request_headers,
                 net::DEFAULT_PRIORITY));

  VerifyHeaderValue(header, value);
}

}  // namespace content
