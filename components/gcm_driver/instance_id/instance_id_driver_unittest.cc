// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_driver.h"

#include <cmath>
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace instance_id {

namespace {

const char kTestAppID1[] = "TestApp1";
const char kTestAppID2[] = "TestApp2";

bool VerifyInstanceID(const std::string& str) {
  // Checks the length.
  if (str.length() != static_cast<size_t>(
          std::ceil(InstanceID::kInstanceIDByteLength * 8 / 6.0)))
    return false;

  // Checks if it is URL-safe base64 encoded.
  for (auto ch : str) {
    if (!IsAsciiAlpha(ch) && !IsAsciiDigit(ch) && ch != '_' && ch != '-')
      return false;
  }
  return true;
}

}  // namespace

class InstanceIDDriverTest : public testing::Test {
 public:
  InstanceIDDriverTest();
  ~InstanceIDDriverTest() override;

  // testing::Test:
  void SetUp() override;

  void WaitForAsyncOperation();

  void DeleteIDCompleted(InstanceID::Result result);

  InstanceIDDriver* driver() const { return driver_.get(); }
  InstanceID::Result delete_id_result() const { return delete_id_result_; }

 private:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<gcm::FakeGCMDriver> gcm_driver_;
  scoped_ptr<InstanceIDDriver> driver_;
  InstanceID::Result delete_id_result_;
  base::Closure async_operation_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDDriverTest);
};

InstanceIDDriverTest::InstanceIDDriverTest()
    : delete_id_result_(InstanceID::UNKNOWN_ERROR) {
}

InstanceIDDriverTest::~InstanceIDDriverTest() {
}

void InstanceIDDriverTest::SetUp() {
  gcm_driver_.reset(new gcm::FakeGCMDriver);
  driver_.reset(new InstanceIDDriver(gcm_driver_.get()));
}

void InstanceIDDriverTest::WaitForAsyncOperation() {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void InstanceIDDriverTest::DeleteIDCompleted(InstanceID::Result result) {
  delete_id_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

TEST_F(InstanceIDDriverTest, NewID) {
  // Creation time should not be set when the ID is not created.
  InstanceID* instance_id1 = driver()->GetInstanceID(kTestAppID1);
  EXPECT_TRUE(instance_id1->GetCreationTime().is_null());

  // New ID is generated for the first time.
  std::string id1 = instance_id1->GetID();
  EXPECT_FALSE(id1.empty());
  EXPECT_TRUE(VerifyInstanceID(id1));
  base::Time creation_time = instance_id1->GetCreationTime();
  EXPECT_FALSE(creation_time.is_null());

  // Same ID is returned for the same app.
  EXPECT_EQ(id1, instance_id1->GetID());
  EXPECT_EQ(creation_time, instance_id1->GetCreationTime());

  // New ID is generated for another app.
  InstanceID* instance_id2 = driver()->GetInstanceID(kTestAppID2);
  std::string id2 = instance_id2->GetID();
  EXPECT_FALSE(id2.empty());
  EXPECT_TRUE(VerifyInstanceID(id2));
  EXPECT_NE(id1, id2);
  EXPECT_FALSE(instance_id2->GetCreationTime().is_null());
}

TEST_F(InstanceIDDriverTest, DeleteID) {
  InstanceID* instance_id = driver()->GetInstanceID(kTestAppID1);
  std::string id1 = instance_id->GetID();
  EXPECT_FALSE(id1.empty());
  EXPECT_FALSE(instance_id->GetCreationTime().is_null());

  // New ID will be generated from GetID after calling DeleteID.
  instance_id->DeleteID(base::Bind(&InstanceIDDriverTest::DeleteIDCompleted,
                                   base::Unretained(this)));
  WaitForAsyncOperation();
  EXPECT_EQ(InstanceID::SUCCESS, delete_id_result());
  EXPECT_TRUE(instance_id->GetCreationTime().is_null());

  std::string id2 = instance_id->GetID();
  EXPECT_FALSE(id2.empty());
  EXPECT_NE(id1, id2);
  EXPECT_FALSE(instance_id->GetCreationTime().is_null());
}

}  // instance_id
