// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_set_volume_job.h"

#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

// Name of the field in the command payload containing the volume.
const char kVolumeFieldName[] = "volume";

int g_volume;

em::RemoteCommand GenerateSetVolumeCommandProto(base::TimeDelta age_of_command,
                                                int volume) {
  em::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_DEVICE_SET_VOLUME);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  std::string payload;
  base::DictionaryValue root_dict;
  root_dict.SetInteger(kVolumeFieldName, volume);
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

void SetVolumeCallback(int volume) {
  g_volume = volume;
}

std::unique_ptr<RemoteCommandJob> CreateSetVolumeJob(
    base::TimeTicks issued_time,
    int volume) {
  auto* job_ptr = new DeviceCommandSetVolumeJob();
  auto job = base::WrapUnique<RemoteCommandJob>(job_ptr);
  job_ptr->SetVolumeCallbackForTesting(base::Bind(&SetVolumeCallback));
  auto set_volume_command_proto = GenerateSetVolumeCommandProto(
      base::TimeTicks::Now() - issued_time, volume);
  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), set_volume_command_proto));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
  return job;
}

}  // namespace

class DeviceCommandSetVolumeTest : public ash::test::AshTestBase {
 public:
  void VerifyResults(RemoteCommandJob* job,
                     RemoteCommandJob::Status expected_status,
                     int expected_volume);

 protected:
  DeviceCommandSetVolumeTest();

  // testing::Test
  void SetUp() override;

  base::RunLoop run_loop_;
  base::TimeTicks test_start_time_;

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandSetVolumeTest);
};

DeviceCommandSetVolumeTest::DeviceCommandSetVolumeTest()
    : task_runner_(new base::TestMockTimeTaskRunner()) {}

void DeviceCommandSetVolumeTest::SetUp() {
  ash::test::AshTestBase::SetUp();
  test_start_time_ = base::TimeTicks::Now();
}

void DeviceCommandSetVolumeTest::VerifyResults(
    RemoteCommandJob* job,
    RemoteCommandJob::Status expected_status,
    int expected_volume) {
  EXPECT_EQ(expected_status, job->status());
  if (job->status() == RemoteCommandJob::SUCCEEDED) {
    EXPECT_EQ(expected_volume, g_volume);
  }
  run_loop_.Quit();
}

TEST_F(DeviceCommandSetVolumeTest, Success) {
  const int kVolume = 45;
  auto job = CreateSetVolumeJob(test_start_time_, kVolume);
  bool success =
      job->Run(base::TimeTicks::Now(),
               base::Bind(&DeviceCommandSetVolumeTest::VerifyResults,
                          base::Unretained(this), base::Unretained(job.get()),
                          RemoteCommandJob::SUCCEEDED, kVolume));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandSetVolumeTest, VolumeOutOfRange) {
  const int kVolume = 110;
  std::unique_ptr<RemoteCommandJob> job(new DeviceCommandSetVolumeJob());
  auto set_volume_command_proto = GenerateSetVolumeCommandProto(
      base::TimeTicks::Now() - test_start_time_, kVolume);
  EXPECT_FALSE(job->Init(base::TimeTicks::Now(), set_volume_command_proto));
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandSetVolumeTest, CommandTimeout) {
  const int kVolume = 45;
  auto delta = base::TimeDelta::FromMinutes(10);
  auto job = CreateSetVolumeJob(test_start_time_ - delta, kVolume);
  bool success =
      job->Run(base::TimeTicks::Now(),
               base::Bind(&DeviceCommandSetVolumeTest::VerifyResults,
                          base::Unretained(this), base::Unretained(job.get()),
                          RemoteCommandJob::SUCCEEDED, kVolume));
  EXPECT_FALSE(success);
}

}  // namespace policy
