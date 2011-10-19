// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host.h"

#include "net/base/ssl_config_service.h"
#include "net/http/http_pipelined_connection.h"
#include "net/proxy/proxy_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::ReturnNull;

static const int kMaxCapacity = 3;

namespace net {

static ClientSocketHandle* kDummyConnection =
    reinterpret_cast<ClientSocketHandle*>(84);
static HttpPipelinedStream* kDummyStream =
    reinterpret_cast<HttpPipelinedStream*>(42);

class MockHostDelegate : public HttpPipelinedHost::Delegate {
 public:
  MOCK_METHOD1(OnHostIdle, void(HttpPipelinedHost* host));
  MOCK_METHOD1(OnHostHasAdditionalCapacity, void(HttpPipelinedHost* host));
};

class MockPipelineFactory : public HttpPipelinedConnection::Factory {
 public:
  MOCK_METHOD6(CreateNewPipeline, HttpPipelinedConnection*(
      ClientSocketHandle* connection,
      HttpPipelinedConnection::Delegate* delegate,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated));
};

class MockPipeline : public HttpPipelinedConnection {
 public:
  MockPipeline(int depth, bool usable, bool active)
      : depth_(depth),
        usable_(usable),
        active_(active) {
  }

  void SetState(int depth, bool usable, bool active) {
    depth_ = depth;
    usable_ = usable;
    active_ = active;
  }

  virtual int depth() const OVERRIDE { return depth_; }
  virtual bool usable() const OVERRIDE { return usable_; }
  virtual bool active() const OVERRIDE { return active_; }

  MOCK_METHOD0(CreateNewStream, HttpPipelinedStream*());
  MOCK_METHOD1(OnStreamDeleted, void(int pipeline_id));
  MOCK_CONST_METHOD0(used_ssl_config, const SSLConfig&());
  MOCK_CONST_METHOD0(used_proxy_info, const ProxyInfo&());
  MOCK_CONST_METHOD0(source, const NetLog::Source&());
  MOCK_CONST_METHOD0(was_npn_negotiated, bool());

 private:
  int depth_;
  bool usable_;
  bool active_;
};

class HttpPipelinedHostTest : public testing::Test {
 public:
  HttpPipelinedHostTest()
      : origin_("host", 123),
        factory_(new MockPipelineFactory),  // Owned by host_.
        host_(&delegate_, origin_, factory_) {
  }

  MockPipeline* AddTestPipeline(int depth, bool usable, bool active) {
    MockPipeline* pipeline = new MockPipeline(depth, usable, active);
    EXPECT_CALL(*factory_, CreateNewPipeline(kDummyConnection, &host_,
                                             Ref(ssl_config_), Ref(proxy_info_),
                                             Ref(net_log_), true))
        .Times(1)
        .WillOnce(Return(pipeline));
    EXPECT_CALL(*pipeline, CreateNewStream())
        .Times(1)
        .WillOnce(Return(kDummyStream));
    EXPECT_EQ(kDummyStream, host_.CreateStreamOnNewPipeline(
        kDummyConnection, ssl_config_, proxy_info_, net_log_, true));
    return pipeline;
  }

  void ClearTestPipeline(MockPipeline* pipeline) {
    pipeline->SetState(0, true, true);
    host_.OnPipelineHasCapacity(pipeline);
  }

  NiceMock<MockHostDelegate> delegate_;
  HostPortPair origin_;
  MockPipelineFactory* factory_;
  HttpPipelinedHost host_;

  SSLConfig ssl_config_;
  ProxyInfo proxy_info_;
  BoundNetLog net_log_;
};

TEST_F(HttpPipelinedHostTest, Delegate) {
  EXPECT_TRUE(origin_.Equals(host_.origin()));
}

TEST_F(HttpPipelinedHostTest, OnHostIdle) {
  MockPipeline* pipeline = AddTestPipeline(0, false, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(&host_))
      .Times(0);
  EXPECT_CALL(delegate_, OnHostIdle(&host_))
      .Times(1);
  host_.OnPipelineHasCapacity(pipeline);
}

TEST_F(HttpPipelinedHostTest, OnHostHasAdditionalCapacity) {
  MockPipeline* pipeline = AddTestPipeline(1, true, true);

  EXPECT_CALL(delegate_, OnHostHasAdditionalCapacity(&host_))
      .Times(2);
  EXPECT_CALL(delegate_, OnHostIdle(&host_))
      .Times(0);

  host_.OnPipelineHasCapacity(pipeline);

  EXPECT_CALL(delegate_, OnHostIdle(&host_))
      .Times(1);
  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostTest, IgnoresUnusablePipeline) {
  MockPipeline* pipeline = AddTestPipeline(1, false, true);

  EXPECT_FALSE(host_.IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_.CreateStreamOnExistingPipeline());

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostTest, IgnoresInactivePipeline) {
  MockPipeline* pipeline = AddTestPipeline(1, true, false);

  EXPECT_FALSE(host_.IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_.CreateStreamOnExistingPipeline());

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostTest, IgnoresFullPipeline) {
  MockPipeline* pipeline = AddTestPipeline(kMaxCapacity, true, true);

  EXPECT_FALSE(host_.IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_.CreateStreamOnExistingPipeline());

  ClearTestPipeline(pipeline);
}

TEST_F(HttpPipelinedHostTest, PicksLeastLoadedPipeline) {
  MockPipeline* full_pipeline = AddTestPipeline(kMaxCapacity, true, true);
  MockPipeline* usable_pipeline = AddTestPipeline(kMaxCapacity - 1, true, true);
  MockPipeline* empty_pipeline = AddTestPipeline(0, true, true);

  EXPECT_TRUE(host_.IsExistingPipelineAvailable());
  EXPECT_CALL(*empty_pipeline, CreateNewStream())
      .Times(1)
      .WillOnce(ReturnNull());
  EXPECT_EQ(NULL, host_.CreateStreamOnExistingPipeline());

  ClearTestPipeline(full_pipeline);
  ClearTestPipeline(usable_pipeline);
  ClearTestPipeline(empty_pipeline);
}

TEST_F(HttpPipelinedHostTest, EmptyPipelineIsRemoved) {
  MockPipeline* empty_pipeline = AddTestPipeline(0, true, true);

  EXPECT_TRUE(host_.IsExistingPipelineAvailable());
  EXPECT_CALL(*empty_pipeline, CreateNewStream())
      .Times(1)
      .WillOnce(Return(kDummyStream));
  EXPECT_EQ(kDummyStream, host_.CreateStreamOnExistingPipeline());

  ClearTestPipeline(empty_pipeline);

  EXPECT_FALSE(host_.IsExistingPipelineAvailable());
  EXPECT_EQ(NULL, host_.CreateStreamOnExistingPipeline());
}

}  // namespace net
