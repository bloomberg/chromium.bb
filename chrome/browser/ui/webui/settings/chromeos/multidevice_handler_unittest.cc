// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace settings {

namespace {

class TestMultideviceHandler : public MultideviceHandler {
 public:
  TestMultideviceHandler(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
      : MultideviceHandler(multidevice_setup_client) {}
  ~TestMultideviceHandler() override = default;

  // Make public for testing.
  using MultideviceHandler::AllowJavascript;
  using MultideviceHandler::RegisterMessages;
  using MultideviceHandler::set_web_ui;
};

void VerifyPageContentDict(
    const base::Value* value,
    multidevice_setup::mojom::HostStatus expected_host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& expected_host_device,
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  const base::DictionaryValue* page_content_dict;
  EXPECT_TRUE(value->GetAsDictionary(&page_content_dict));

  int mode;
  EXPECT_TRUE(page_content_dict->GetInteger("mode", &mode));
  EXPECT_EQ(static_cast<int>(expected_host_status), mode);

  int better_together_state;
  EXPECT_TRUE(page_content_dict->GetInteger("betterTogetherState",
                                            &better_together_state));
  auto it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite);
  EXPECT_EQ(static_cast<int>(it->second), better_together_state);

  int instant_tethering_state;
  EXPECT_TRUE(page_content_dict->GetInteger("instantTetheringState",
                                            &instant_tethering_state));
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kInstantTethering);
  EXPECT_EQ(static_cast<int>(it->second), instant_tethering_state);

  int messages_state;
  EXPECT_TRUE(page_content_dict->GetInteger("messagesState", &messages_state));
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kMessages);
  EXPECT_EQ(static_cast<int>(it->second), messages_state);

  int smart_lock_state;
  EXPECT_TRUE(
      page_content_dict->GetInteger("smartLockState", &smart_lock_state));
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kSmartLock);
  EXPECT_EQ(static_cast<int>(it->second), smart_lock_state);

  std::string host_device_name;
  if (expected_host_device) {
    EXPECT_TRUE(
        page_content_dict->GetString("hostDeviceName", &host_device_name));
    EXPECT_EQ(expected_host_device->name(), host_device_name);
  } else {
    EXPECT_FALSE(
        page_content_dict->GetString("hostDeviceName", &host_device_name));
  }
}

}  // namespace

class MultideviceHandlerTest : public testing::Test {
 protected:
  MultideviceHandlerTest()
      : test_device_(cryptauth::CreateRemoteDeviceRefForTest()) {}
  ~MultideviceHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_web_ui_ = std::make_unique<content::TestWebUI>();

    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();

    handler_ = std::make_unique<TestMultideviceHandler>(
        fake_multidevice_setup_client_.get());
    handler_->set_web_ui(test_web_ui_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascript();
  }

  void SetPageContent(
      multidevice_setup::mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device,
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) {
    current_host_status_with_device_ = std::make_pair(host_status, host_device);
    current_feature_states_map_ = feature_states_map;
  }

  void CallGetPageContentData(bool expected_to_request_data_from_device_sync) {
    EXPECT_TRUE(current_host_status_with_device_);
    EXPECT_TRUE(current_host_status_with_device_);

    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getPageContentData", &args);

    if (expected_to_request_data_from_device_sync) {
      // The callback did not complete yet, so no call should have been made.
      EXPECT_EQ(call_data_count_before_call, test_web_ui()->call_data().size());

      // Invoke the host status callback.
      fake_multidevice_setup_client()->InvokePendingGetHostStatusCallback(
          current_host_status_with_device_->first /* host_status */,
          current_host_status_with_device_->second /* host_device */);

      // Invoke the feature states callback; this should trigger the event to be
      // sent to JS.
      fake_multidevice_setup_client()->InvokePendingGetFeatureStatesCallback(
          *current_feature_states_map_);
    }

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    VerifyPageContent(call_data.arg3());
  }

  void SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
    current_host_status_with_device_ = std::make_pair(host_status, host_device);
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->NotifyHostStatusChanged(host_status,
                                                            host_device);
    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulateFeatureStatesUpdate(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) {
    current_feature_states_map_ = feature_states_map;
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->NotifyFeatureStateChanged(
        feature_states_map);
    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void CallRetryPendingHostSetup(bool success) {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("retryPendingHostSetup", &empty_args);
    fake_multidevice_setup_client()->InvokePendingRetrySetHostNowCallback(
        success);
  }

  void CallSetFeatureEnabledState(multidevice_setup::mojom::Feature feature,
                                  bool enabled,
                                  bool success) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    args.AppendInteger(static_cast<int>(feature));
    args.AppendBoolean(enabled);

    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("setFeatureEnabledState", &args);
    fake_multidevice_setup_client()
        ->InvokePendingSetFeatureEnabledStateCallback(
            feature /* expected_feature */, enabled /* expected_enabled */,
            success);

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    EXPECT_EQ(success, call_data.arg3()->GetBool());
  }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *test_web_ui_->call_data()[index];
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return fake_multidevice_setup_client_.get();
  }

  const cryptauth::RemoteDeviceRef test_device_;

 private:
  void VerifyPageContent(const base::Value* value) {
    VerifyPageContentDict(value, current_host_status_with_device_->first,
                          current_host_status_with_device_->second,
                          *current_feature_states_map_);
  }

  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<TestMultideviceHandler> handler_;

  base::Optional<std::pair<multidevice_setup::mojom::HostStatus,
                           base::Optional<cryptauth::RemoteDeviceRef>>>
      current_host_status_with_device_;
  base::Optional<multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap>
      current_feature_states_map_;

  DISALLOW_COPY_AND_ASSIGN(MultideviceHandlerTest);
};

TEST_F(MultideviceHandlerTest, PageContentData) {
  static multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map{
          {multidevice_setup::mojom::Feature::kBetterTogetherSuite,
           multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
          {multidevice_setup::mojom::Feature::kInstantTethering,
           multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
          {multidevice_setup::mojom::Feature::kMessages,
           multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
          {multidevice_setup::mojom::Feature::kSmartLock,
           multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost}};
  // multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
  //     feature_states_map = GenerateUnverifiedFeatureStatesMap();

  SetPageContent(multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
                 base::nullopt /* host_device */, feature_states_map);
  CallGetPageContentData(true /* expected_to_request_data_from_device_sync */);
  CallGetPageContentData(false /* expected_to_request_data_from_device_sync */);

  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      base::nullopt /* host_device */);
  SimulateHostStatusUpdate(multidevice_setup::mojom::HostStatus::
                               kHostSetLocallyButWaitingForBackendConfirmation,
                           test_device_);
  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kHostSetButNotYetVerified,
      test_device_);
  SimulateHostStatusUpdate(multidevice_setup::mojom::HostStatus::kHostVerified,
                           test_device_);

  feature_states_map[multidevice_setup::mojom::Feature::kBetterTogetherSuite] =
      multidevice_setup::mojom::FeatureState::kEnabledByUser;
  SimulateFeatureStatesUpdate(feature_states_map);

  feature_states_map[multidevice_setup::mojom::Feature::kBetterTogetherSuite] =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;
  SimulateFeatureStatesUpdate(feature_states_map);
}

TEST_F(MultideviceHandlerTest, RetryPendingHostSetup) {
  CallRetryPendingHostSetup(true /* success */);
  CallRetryPendingHostSetup(false /* success */);
}

TEST_F(MultideviceHandlerTest, SetFeatureEnabledState) {
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      true /* enabled */, true /* success */);
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      false /* enabled */, false /* success */);
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      false /* enabled */, true /* success */);
}

}  // namespace settings

}  // namespace chromeos
