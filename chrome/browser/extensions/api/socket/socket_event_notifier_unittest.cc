// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::SaveArg;

namespace extensions {

class MockExtensionEventRouter : public ExtensionEventRouter {
 public:
  explicit MockExtensionEventRouter(Profile* profile) :
      ExtensionEventRouter(profile) {}

  MOCK_METHOD5(DispatchEventToExtension, void(const std::string& extension_id,
                                              const std::string& event_name,
                                              const std::string& event_args,
                                              Profile* source_profile,
                                              const GURL& event_url));


 private:
  DISALLOW_COPY_AND_ASSIGN(MockExtensionEventRouter);
};

class MockTestingProfile : public TestingProfile {
 public:
  MockTestingProfile() {}
  MOCK_METHOD0(GetExtensionEventRouter, ExtensionEventRouter*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTestingProfile);
};

class SocketEventNotifierTest : public testing::Test {
 public:
  SocketEventNotifierTest()
      : mock_event_router_(
          new MockExtensionEventRouter(&profile_)),
        src_id_(42) {
    EXPECT_TRUE(Extension::GenerateId("chrome fast", &extension_id_));
    event_notifier_.reset(new SocketEventNotifier(mock_event_router_.get(),
                                                  &profile_,
                                                  extension_id_,
                                                  src_id_, url_));
  }

  MockTestingProfile profile_;
  scoped_ptr<MockExtensionEventRouter> mock_event_router_;
  int src_id_;
  GURL url_;
  std::string extension_id_;
  scoped_ptr<SocketEventNotifier> event_notifier_;
};

TEST_F(SocketEventNotifierTest, TestOnConnectCompleteEvent) {
  std::string event_args;
  EXPECT_CALL(*mock_event_router_.get(),
              DispatchEventToExtension(
                  extension_id_,
                  events::kOnSocketEvent,
                  _,
                  &profile_,
                  url_))
      .Times(1)
      .WillOnce(SaveArg<2>(&event_args));

  const int expected_result_code = 777;
  event_notifier_->OnConnectComplete(expected_result_code);

  scoped_ptr<Value> result(base::JSONReader::Read(event_args, true));
  Value* value = result.get();
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_EQ(Value::TYPE_LIST, value->GetType());
  ListValue* list = static_cast<ListValue*>(value);
  ASSERT_EQ(1u, list->GetSize());

  DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_src_id = 0;
  ASSERT_TRUE(info->GetInteger("srcId", &tmp_src_id));
  ASSERT_EQ(src_id_, tmp_src_id);

  int actual_result_code = 0;
  ASSERT_TRUE(info->GetInteger("resultCode", &actual_result_code));
  ASSERT_EQ(expected_result_code, actual_result_code);
}

TEST_F(SocketEventNotifierTest, TestOnWriteCompleteEvent) {
  std::string event_args;
  EXPECT_CALL(*mock_event_router_.get(),
              DispatchEventToExtension(
                  extension_id_,
                  events::kOnSocketEvent,
                  _,
                  &profile_,
                  url_))
      .Times(1)
      .WillOnce(SaveArg<2>(&event_args));

  const int expected_result_code = 888;
  event_notifier_->OnWriteComplete(expected_result_code);

  scoped_ptr<Value> result(base::JSONReader::Read(event_args, true));
  Value* value = result.get();
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_EQ(Value::TYPE_LIST, value->GetType());
  ListValue* list = static_cast<ListValue*>(value);
  ASSERT_EQ(1u, list->GetSize());

  DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_src_id = 0;
  ASSERT_TRUE(info->GetInteger("srcId", &tmp_src_id));
  ASSERT_EQ(src_id_, tmp_src_id);

  int actual_result_code = 0;
  ASSERT_TRUE(info->GetInteger("resultCode", &actual_result_code));
  ASSERT_EQ(expected_result_code, actual_result_code);
}

TEST_F(SocketEventNotifierTest, TestOnDataReadEvent) {
  std::string event_args;
  EXPECT_CALL(*mock_event_router_.get(),
              DispatchEventToExtension(
                  extension_id_,
                  events::kOnSocketEvent,
                  _,
                  &profile_,
                  url_))
      .Times(1)
      .WillOnce(SaveArg<2>(&event_args));

  const int expected_result_code = 888;
  const std::string result_data("hi");
  event_notifier_->OnDataRead(expected_result_code, result_data);

  scoped_ptr<Value> result(base::JSONReader::Read(event_args, true));
  Value* value = result.get();
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_EQ(Value::TYPE_LIST, value->GetType());
  ListValue* list = static_cast<ListValue*>(value);
  ASSERT_EQ(1u, list->GetSize());

  DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_src_id = 0;
  ASSERT_TRUE(info->GetInteger("srcId", &tmp_src_id));
  ASSERT_EQ(src_id_, tmp_src_id);

  int actual_result_code = 0;
  ASSERT_TRUE(info->GetInteger("resultCode", &actual_result_code));
  ASSERT_EQ(expected_result_code, actual_result_code);

  std::string tmp_result_data;
  ASSERT_TRUE(info->GetString("data", &tmp_result_data));
  ASSERT_EQ(result_data, tmp_result_data);
}

}  // namespace extensions
