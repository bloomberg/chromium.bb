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
    const base::Optional<cryptauth::RemoteDeviceRef>& expected_host_device) {
  const base::DictionaryValue* page_content_dict;
  EXPECT_TRUE(value->GetAsDictionary(&page_content_dict));

  int mode;
  EXPECT_TRUE(page_content_dict->GetInteger("mode", &mode));
  EXPECT_EQ(static_cast<int>(expected_host_status), mode);

  if (expected_host_device) {
    const base::DictionaryValue* remote_device_dict;
    EXPECT_TRUE(
        page_content_dict->GetDictionary("hostDevice", &remote_device_dict));

    std::string name;
    EXPECT_TRUE(remote_device_dict->GetString("name", &name));
    EXPECT_EQ(expected_host_device->name(), name);
  } else {
    EXPECT_FALSE(page_content_dict->GetDictionary("hostDevice", nullptr));
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

  void CallGetPageContentData(
      multidevice_setup::mojom::HostStatus expected_host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& expected_host_device) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getPageContentData", &args);

    // The callback did not complete yet, so no call should have been made.
    EXPECT_EQ(call_data_count_before_call, test_web_ui()->call_data().size());

    // Invoke the callback; this should trigger the event to be sent to JS.
    fake_multidevice_setup_client()->InvokePendingGetHostStatusCallback(
        expected_host_status, expected_host_device);
    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    VerifyPageContentDict(call_data.arg3(), expected_host_status,
                          expected_host_device);
  }

  void SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
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
    VerifyPageContentDict(call_data.arg2(), host_status, host_device);
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
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<TestMultideviceHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(MultideviceHandlerTest);
};

TEST_F(MultideviceHandlerTest, PageContentData) {
  CallGetPageContentData(multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
                         base::nullopt /* host_device */);
  CallGetPageContentData(
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      base::nullopt /* host_device */);
  CallGetPageContentData(multidevice_setup::mojom::HostStatus::
                             kHostSetLocallyButWaitingForBackendConfirmation,
                         test_device_);
  CallGetPageContentData(
      multidevice_setup::mojom::HostStatus::kHostSetButNotYetVerified,
      test_device_);
  CallGetPageContentData(multidevice_setup::mojom::HostStatus::kHostVerified,
                         test_device_);
}

TEST_F(MultideviceHandlerTest, HostStatusUpdates) {
  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
      base::nullopt /* host_device */);
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
}

}  // namespace settings

}  // namespace chromeos
