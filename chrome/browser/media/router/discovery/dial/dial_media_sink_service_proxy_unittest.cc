// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_proxy.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace media_router {

class MockDialMediaSinkServiceImpl : public DialMediaSinkServiceImpl {
 public:
  MockDialMediaSinkServiceImpl(
      const MediaSinkService::OnSinksDiscoveredCallback&
          sink_discovery_callback,
      net::URLRequestContextGetter* request_context)
      : DialMediaSinkServiceImpl(sink_discovery_callback, request_context) {}
  ~MockDialMediaSinkServiceImpl() override {}

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());

  void OnSinksDiscovered() {
    sink_discovery_callback_.Run(std::vector<MediaSinkInternal>());
  }
};

class DialMediaSinkServiceProxyTest : public ::testing::Test {
 public:
  DialMediaSinkServiceProxyTest()
      : proxy_(new DialMediaSinkServiceProxy(mock_sink_discovered_cb_.Get(),
                                             &profile_)) {
    std::unique_ptr<MockDialMediaSinkServiceImpl> mock_dial_media_sink_service =
        base::MakeUnique<MockDialMediaSinkServiceImpl>(
            base::Bind(&DialMediaSinkServiceProxy::OnSinksDiscoveredOnIOThread,
                       base::Unretained(proxy_.get())),
            profile_.GetRequestContext());
    mock_service_ = mock_dial_media_sink_service.get();
    proxy_->SetDialMediaSinkServiceForTest(
        std::move(mock_dial_media_sink_service));
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_cb_;

  MockDialMediaSinkServiceImpl* mock_service_;
  scoped_refptr<DialMediaSinkServiceProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkServiceProxyTest);
};

TEST_F(DialMediaSinkServiceProxyTest, TestStart) {
  EXPECT_CALL(*mock_service_, Start()).WillOnce(InvokeWithoutArgs([]() {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  }));

  proxy_->Start();
}

TEST_F(DialMediaSinkServiceProxyTest, TestStop) {
  EXPECT_CALL(*mock_service_, Stop()).WillOnce(InvokeWithoutArgs([]() {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  }));

  proxy_->Stop();
}

TEST_F(DialMediaSinkServiceProxyTest, TestOnSinksDiscovered) {
  EXPECT_CALL(*mock_service_, Start());
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).Times(1);

  proxy_->Start();
  mock_service_->OnSinksDiscovered();
}

TEST_F(DialMediaSinkServiceProxyTest, TestForceSinkDiscoveryCallback) {
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_));

  proxy_->ForceSinkDiscoveryCallback();
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
