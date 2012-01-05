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

class SocketEventNotifierTest : public testing::Test {
};

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

TEST_F(SocketEventNotifierTest, TestBasicOperation) {
  MockTestingProfile profile;
  scoped_ptr<MockExtensionEventRouter> mock_event_router(
      new MockExtensionEventRouter(&profile));

  std::string extension_id;
  EXPECT_TRUE(Extension::GenerateId("e^(iπ)+1=0", &extension_id));
  int src_id = 42;
  GURL url;
  scoped_ptr<SocketEventNotifier> event_notifier(
      new SocketEventNotifier(mock_event_router.get(), &profile,
                              extension_id, src_id, url));

  std::string event_args;
  EXPECT_CALL(*mock_event_router.get(),
              DispatchEventToExtension(
                  extension_id,
                  events::kOnSocketEvent,
                  _,
                  &profile,
                  url))
      .Times(1)
      .WillOnce(SaveArg<2>(&event_args));

  const int result_code = 888;
  event_notifier->OnWriteComplete(result_code);

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
  ASSERT_EQ(src_id, tmp_src_id);

  int tmp_result_code = 0;
  ASSERT_TRUE(info->GetInteger("resultCode", &tmp_result_code));
  ASSERT_EQ(result_code, tmp_result_code);
}

}  // namespace extensions
