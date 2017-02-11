// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/serializable_user_data_manager.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {
// User Data and Key to use for tests.
NSString* const kTestUserData = @"TestUserData";
NSString* const kTestUserDataKey = @"TestUserDataKey";
}  // namespace

class SerializableUserDataManagerTest : public PlatformTest {
 protected:
  // Convenience getter for the user data manager.
  web::SerializableUserDataManager* manager() {
    return web::SerializableUserDataManager::FromWebState(&web_state_);
  }

  web::TestWebState web_state_;
};

// Tests that serializable data can be successfully added and read.
TEST_F(SerializableUserDataManagerTest, SetAndReadData) {
  manager()->AddSerializableData(kTestUserData, kTestUserDataKey);
  id value = manager()->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(value, kTestUserData);
}

// Tests that SerializableUserData can successfully encode and decode.
TEST_F(SerializableUserDataManagerTest, EncodeDecode) {
  // Create a SerializableUserData instance for the test data.
  manager()->AddSerializableData(kTestUserData, kTestUserDataKey);
  std::unique_ptr<web::SerializableUserData> user_data =
      manager()->CreateSerializableUserData();

  // Archive the serializable user data.
  base::scoped_nsobject<NSMutableData> data([[NSMutableData alloc] init]);
  base::scoped_nsobject<NSKeyedArchiver> archiver(
      [[NSKeyedArchiver alloc] initForWritingWithMutableData:data]);
  user_data->Encode(archiver);
  [archiver finishEncoding];

  // Create a new SerializableUserData by unarchiving.
  base::scoped_nsobject<NSKeyedUnarchiver> unarchiver(
      [[NSKeyedUnarchiver alloc] initForReadingWithData:data]);
  std::unique_ptr<web::SerializableUserData> decoded_data =
      web::SerializableUserData::Create();
  decoded_data->Decode(unarchiver);

  // Add the decoded user data to a new WebState and verify its contents.
  web::TestWebState decoded_web_state;
  web::SerializableUserDataManager* decoded_manager =
      web::SerializableUserDataManager::FromWebState(&decoded_web_state);
  decoded_manager->AddSerializableUserData(decoded_data.get());
  id decoded_value =
      decoded_manager->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(decoded_value, kTestUserData);
}
