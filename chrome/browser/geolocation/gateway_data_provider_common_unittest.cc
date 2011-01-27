// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/gateway_data_provider_common.h"

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::Invoke;
using testing::Return;

namespace {

class MockGatewayApi : public GatewayDataProviderCommon::GatewayApiInterface {
 public:
  MockGatewayApi() {
    ON_CALL(*this, GetRouterData(_))
        .WillByDefault(Invoke(this, &MockGatewayApi::GetRouterDataInternal));
  }

  MOCK_METHOD1(GetRouterData, bool(GatewayData::RouterDataSet* data));

  GatewayData::RouterDataSet data_out_;

 private:
  bool GetRouterDataInternal(GatewayData::RouterDataSet* data) {
    *data = data_out_;
    return true;
  }
};

// Stops the specified (nested) message loop when the listener is called back.
class MessageLoopQuitListener
    : public GatewayDataProviderCommon::ListenerInterface {
 public:
  explicit MessageLoopQuitListener(MessageLoop* message_loop)
      : message_loop_to_quit_(message_loop) {
    CHECK(message_loop_to_quit_);
  }

  // ListenerInterface
  virtual void DeviceDataUpdateAvailable(
      DeviceDataProvider<GatewayData>* provider) {
    // Provider should call back on client's thread.
    EXPECT_EQ(MessageLoop::current(), message_loop_to_quit_);
    provider_ = provider;
    message_loop_to_quit_->QuitNow();
  }

  MessageLoop* message_loop_to_quit_;
  DeviceDataProvider<GatewayData>* provider_;
};

class MockGatewayPollingPolicy : public GatewayPollingPolicyInterface {
 public:
  MockGatewayPollingPolicy() {
    ON_CALL(*this, PollingInterval())
        .WillByDefault(Return(1));
    ON_CALL(*this, NoRouterInterval())
        .WillByDefault(Return(1));
  }

  // GatewayPollingPolicyInterface
  MOCK_METHOD0(PollingInterval, int());
  MOCK_METHOD0(NoRouterInterval, int());
 };

class GatewayDataProviderCommonWithMock : public GatewayDataProviderCommon {
 public:
  GatewayDataProviderCommonWithMock()
      : new_gateway_api_(new MockGatewayApi),
        new_polling_policy_(new MockGatewayPollingPolicy){
  }

  // GatewayDataProviderCommon
  virtual GatewayApiInterface* NewGatewayApi() {
    CHECK(new_gateway_api_ != NULL);
    return new_gateway_api_.release();
  }
  virtual GatewayPollingPolicyInterface* NewPollingPolicy() {
    CHECK(new_polling_policy_ != NULL);
    return new_polling_policy_.release();
  }

  scoped_ptr<MockGatewayApi> new_gateway_api_;
  scoped_ptr<MockGatewayPollingPolicy> new_polling_policy_;

  DISALLOW_COPY_AND_ASSIGN(GatewayDataProviderCommonWithMock);
};

// Main test fixture.
class GeolocationGatewayDataProviderCommonTest : public testing::Test {
 public:
  GeolocationGatewayDataProviderCommonTest()
      : quit_listener_(&main_message_loop_) {
  }

  virtual void SetUp() {
    provider_ = new GatewayDataProviderCommonWithMock;
    gateway_api_ = provider_->new_gateway_api_.get();
    polling_policy_ = provider_->new_polling_policy_.get();
    provider_->AddListener(&quit_listener_);
  }
  virtual void TearDown() {
    provider_->RemoveListener(&quit_listener_);
    provider_->StopDataProvider();
    provider_ = NULL;
  }

 protected:
  MessageLoop main_message_loop_;
  MessageLoopQuitListener quit_listener_;
  scoped_refptr<GatewayDataProviderCommonWithMock> provider_;
  MockGatewayApi* gateway_api_;
  MockGatewayPollingPolicy* polling_policy_;
};

TEST_F(GeolocationGatewayDataProviderCommonTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_EQ(&main_message_loop_, MessageLoop::current());
  EXPECT_TRUE(NULL != provider_.get());
  EXPECT_TRUE(NULL != gateway_api_);
}

TEST_F(GeolocationGatewayDataProviderCommonTest, StartThread) {
  EXPECT_CALL(*gateway_api_, GetRouterData(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  SUCCEED();
}

TEST_F(GeolocationGatewayDataProviderCommonTest, ConnectToDifferentRouter){
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, NoRouterInterval())
      .Times(1);
  EXPECT_CALL(*gateway_api_, GetRouterData(_))
      .WillOnce(Return(true)) // Connected to router.
      .WillOnce(Return(false)) // Disconnected from router.
      .WillRepeatedly(DoDefault()); // Connected to a different router.

  // Set MAC address for output.
  RouterData single_router;
  single_router.mac_address = ASCIIToUTF16("12-34-56-78-54-32");
  gateway_api_->data_out_.insert(single_router);

  provider_->StartDataProvider();
  main_message_loop_.Run();
  main_message_loop_.Run();
}

TEST_F(GeolocationGatewayDataProviderCommonTest, DoAnEmptyScan) {
  EXPECT_CALL(*gateway_api_, GetRouterData(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  GatewayData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(0, static_cast<int>(data.router_data.size()));
}

TEST_F(GeolocationGatewayDataProviderCommonTest, DoScanWithResults) {
  EXPECT_CALL(*gateway_api_, GetRouterData(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      .Times(AtLeast(1));
  RouterData single_router;
  single_router.mac_address = ASCIIToUTF16("12-34-56-78-54-32");
  gateway_api_->data_out_.insert(single_router);
  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  GatewayData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(1, static_cast<int>(data.router_data.size()));
  EXPECT_EQ(single_router.mac_address, data.router_data.begin()->mac_address);
}

}  // namespace
