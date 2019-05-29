// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {
namespace ime {

namespace {

const char kInvalidImeSpec[] = "ime_spec_never_support";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

void TestProcessTextCallback(std::string* res_out,
                             const std::string& response) {
  *res_out = response;
}

class TestClientChannel : mojom::InputChannel {
 public:
  TestClientChannel() : receiver_(this) {}

  mojo::PendingRemote<mojom::InputChannel> CreatePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::InputChannel implementation.
  MOCK_METHOD2(ProcessText, void(const std::string&, ProcessTextCallback));
  MOCK_METHOD2(ProcessMessage,
               void(const std::vector<uint8_t>& message,
                    ProcessMessageCallback));

 private:
  mojo::Receiver<mojom::InputChannel> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestClientChannel);
};

class ImeServiceTest : public testing::Test {
 public:
  ImeServiceTest()
      : service_(
            test_connector_factory_.RegisterInstance(mojom::kServiceName)) {}
  ~ImeServiceTest() override = default;

  MOCK_METHOD1(SentTextCallback, void(const std::string&));
  MOCK_METHOD1(SentMessageCallback, void(const std::vector<uint8_t>&));

 protected:
  void SetUp() override {
    test_connector_factory_.GetDefaultConnector()->Connect(
        mojom::kServiceName, remote_manager_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::InputEngineManager> remote_manager_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestConnectorFactory test_connector_factory_;
  ImeService service_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

}  // namespace

// Tests that the service is instantiated and it will return false when
// activating an IME engine with an invalid IME spec.
TEST_F(ImeServiceTest, ConnectInvalidImeEngine) {
  bool success = true;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kInvalidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_FALSE(success);
}

TEST_F(ImeServiceTest, MultipleClients) {
  bool success = false;
  TestClientChannel test_channel_1;
  TestClientChannel test_channel_2;
  mojo::Remote<mojom::InputChannel> remote_engine_1;
  mojo::Remote<mojom::InputChannel> remote_engine_2;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_1.BindNewPipeAndPassReceiver(),
      test_channel_1.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_2.BindNewPipeAndPassReceiver(),
      test_channel_2.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  std::string response;
  std::string process_text_key =
      "{\"method\":\"keyEvent\",\"type\":\"keydown\""
      ",\"code\":\"KeyA\",\"shift\":true,\"altgr\":false,\"caps\":false}";
  remote_engine_1->ProcessText(
      process_text_key, base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_1.FlushForTesting();

  remote_engine_2->ProcessText(
      process_text_key, base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_2.FlushForTesting();

  std::string process_text_key_count = "{\"method\":\"countKey\"}";
  remote_engine_1->ProcessText(
      process_text_key_count,
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_1.FlushForTesting();
  EXPECT_EQ("1", response);

  remote_engine_2->ProcessText(
      process_text_key_count,
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_2.FlushForTesting();
  EXPECT_EQ("1", response);
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  std::string response;
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyA\","
      "\"shift\":true,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  const wchar_t* expected_response =
      L"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      L"\"arguments\":[\"\u0650\"]}]}";
  EXPECT_EQ(base::WideToUTF8(expected_response), response);

  // Test KeyB.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyB\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      L"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      L"\"arguments\":[\"\u0644\u0627\"]}]}";
  EXPECT_EQ(base::WideToUTF8(expected_response), response);

  // Test unhandled key.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Enter\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);

  // Test keyup.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keyup\",\"code\":\"Enter\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);

  // Test reset.
  remote_engine->ProcessText(
      "{\"method\":\"reset\"}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":true}", response);

  // Test invalid request.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\"}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      "m17n:deva_phone", remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  std::string response;

  // KeyN.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyN\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  const char* expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\u0928\"]}]}";
  EXPECT_EQ(expected_response, response);

  // Backspace.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Backspace\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\"]}]}";
  EXPECT_EQ(expected_response, response);

  // KeyN + KeyC.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyN\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyC\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\u091e\u094d\u091a\"]}]}";
  EXPECT_EQ(expected_response, response);

  // Space.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Space\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      u8"\"arguments\":[\"\u091e\u094d\u091a \"]}]}";
  EXPECT_EQ(expected_response, response);
}

}  // namespace ime
}  // namespace chromeos
