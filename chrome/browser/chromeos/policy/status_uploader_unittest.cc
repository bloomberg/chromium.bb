// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_uploader.h"

#include <utility>

#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/platform_event_types.h"

#if defined(USE_X11)
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/test/events_test_utils_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#endif

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace em = enterprise_management;

namespace {

class MockDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  explicit MockDeviceStatusCollector(PrefService* local_state)
      : DeviceStatusCollector(
            local_state,
            nullptr,
            policy::DeviceStatusCollector::LocationUpdateRequester(),
            policy::DeviceStatusCollector::VolumeInfoFetcher(),
            policy::DeviceStatusCollector::CPUStatisticsFetcher(),
            policy::DeviceStatusCollector::CPUTempFetcher()) {}

  MOCK_METHOD1(GetDeviceStatus, bool(em::DeviceStatusReportRequest*));
  MOCK_METHOD1(GetDeviceSessionStatus, bool(em::SessionStatusReportRequest*));

  // Explicit mock implementation declared here, since gmock::Invoke can't
  // handle returning non-moveable types like scoped_ptr.
  scoped_ptr<policy::DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo()
      override {
    return make_scoped_ptr(new policy::DeviceLocalAccount(
        policy::DeviceLocalAccount::TYPE_KIOSK_APP, "account_id", "app_id",
        "update_url"));
  }
};

}  // namespace

namespace policy {
class StatusUploaderTest : public testing::Test {
 public:
  StatusUploaderTest() : task_runner_(new base::TestSimpleTaskRunner()) {
    DeviceStatusCollector::RegisterPrefs(prefs_.registry());
  }

  void SetUp() override {
#if defined(USE_X11)
    ui::DeviceDataManagerX11::CreateInstance();
#endif
    client_.SetDMToken("dm_token");
    collector_.reset(new MockDeviceStatusCollector(&prefs_));
    settings_helper_.ReplaceProvider(chromeos::kReportUploadFrequency);
  }

  void TearDown() override {
    content::RunAllBlockingPoolTasksUntilIdle();
  }

  // Given a pending task to upload status, mocks out a server response.
  void RunPendingUploadTaskAndCheckNext(const StatusUploader& uploader,
                                        base::TimeDelta expected_delay) {
    EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
    CloudPolicyClient::StatusCallback callback;
    EXPECT_CALL(client_, UploadDeviceStatus(_, _, _))
        .WillOnce(SaveArg<2>(&callback));
    task_runner_->RunPendingTasks();
    testing::Mock::VerifyAndClearExpectations(&device_management_service_);

    // Make sure no status upload is queued up yet (since an upload is in
    // progress).
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());

    // Now invoke the response.
    callback.Run(true);

    // Now that the previous request was satisfied, a task to do the next
    // upload should be queued.
    EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());

    CheckPendingTaskDelay(uploader, expected_delay);
  }

  void CheckPendingTaskDelay(const StatusUploader& uploader,
                             base::TimeDelta expected_delay) {
    // The next task should be scheduled sometime between |last_upload| +
    // |expected_delay| and |now| + |expected_delay|.
    base::Time now = base::Time::NowFromSystemTime();
    base::Time next_task = now + task_runner_->NextPendingTaskDelay();

    EXPECT_LE(next_task, now + expected_delay);
    EXPECT_GE(next_task, uploader.last_upload() + expected_delay);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  scoped_ptr<MockDeviceStatusCollector> collector_;
  ui::UserActivityDetector detector_;
  MockCloudPolicyClient client_;
  MockDeviceManagementService device_management_service_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(StatusUploaderTest, BasicTest) {
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());
  // On startup, first update should happen immediately.
  EXPECT_EQ(base::TimeDelta(), task_runner_->NextPendingTaskDelay());
}

TEST_F(StatusUploaderTest, DifferentFrequencyAtStart) {
  // Keep a pointer to the mock collector because collector_ gets cleared
  // when it is passed to the StatusUploader constructor below.
  MockDeviceStatusCollector* const mock_collector = collector_.get();
  const int new_delay = StatusUploader::kDefaultUploadDelayMs * 2;
  settings_helper_.SetInteger(chromeos::kReportUploadFrequency, new_delay);
  const base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(
      new_delay);
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  ASSERT_EQ(1U, task_runner_->GetPendingTasks().size());
  // On startup, first update should happen immediately.
  EXPECT_EQ(base::TimeDelta(), task_runner_->NextPendingTaskDelay());

  EXPECT_CALL(*mock_collector, GetDeviceStatus(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_collector, GetDeviceSessionStatus(_)).WillRepeatedly(
      Return(true));
  // Second update should use the delay specified in settings.
  RunPendingUploadTaskAndCheckNext(uploader, expected_delay);
}

TEST_F(StatusUploaderTest, ResetTimerAfterStatusCollection) {
  // Keep a pointer to the mock collector because collector_ gets cleared
  // when it is passed to the StatusUploader constructor below.
  MockDeviceStatusCollector* const mock_collector = collector_.get();
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  EXPECT_CALL(*mock_collector, GetDeviceStatus(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_collector, GetDeviceSessionStatus(_)).WillRepeatedly(
      Return(true));
  const base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(
      StatusUploader::kDefaultUploadDelayMs);
  RunPendingUploadTaskAndCheckNext(uploader, expected_delay);

  // Handle this response also, and ensure new task is queued.
  RunPendingUploadTaskAndCheckNext(uploader, expected_delay);

  // Now that the previous request was satisfied, a task to do the next
  // upload should be queued again.
  EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());
}

TEST_F(StatusUploaderTest, ResetTimerAfterFailedStatusCollection) {
  // Keep a pointer to the mock collector because collector_ gets cleared
  // when it is passed to the StatusUploader constructor below.
  MockDeviceStatusCollector* mock_collector = collector_.get();
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  EXPECT_CALL(*mock_collector, GetDeviceStatus(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock_collector, GetDeviceSessionStatus(_)).WillOnce(
      Return(false));
  task_runner_->RunPendingTasks();

  // Make sure the next status upload is queued up.
  EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());
  const base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(
      StatusUploader::kDefaultUploadDelayMs);
  CheckPendingTaskDelay(uploader, expected_delay);
}

TEST_F(StatusUploaderTest, ChangeFrequency) {
  // Keep a pointer to the mock collector because collector_ gets cleared
  // when it is passed to the StatusUploader constructor below.
  MockDeviceStatusCollector* const mock_collector = collector_.get();
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  EXPECT_CALL(*mock_collector, GetDeviceStatus(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_collector, GetDeviceSessionStatus(_)).WillRepeatedly(
      Return(true));
  // Change the frequency. The new frequency should be reflected in the timing
  // used for the next callback.
  const int new_delay = StatusUploader::kDefaultUploadDelayMs * 2;
  settings_helper_.SetInteger(chromeos::kReportUploadFrequency, new_delay);
  const base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(
      new_delay);
  RunPendingUploadTaskAndCheckNext(uploader, expected_delay);
}

#if defined(USE_X11) || defined(USE_OZONE)
TEST_F(StatusUploaderTest, NoUploadAfterUserInput) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  // Should allow data upload before there is user input.
  EXPECT_TRUE(uploader.IsSessionDataUploadAllowed());

// Now mock user input, and no session data should be allowed.
#if defined(USE_X11)
  ui::ScopedXI2Event native_event;
  const int kPointerDeviceId = 10;
  std::vector<int> device_list;
  device_list.push_back(kPointerDeviceId);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);
  native_event.InitGenericButtonEvent(
      kPointerDeviceId, ui::ET_MOUSE_PRESSED, gfx::Point(),
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN);
#elif defined(USE_OZONE)
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  const ui::PlatformEvent& native_event = &e;
#endif
  ui::UserActivityDetector::Get()->DidProcessEvent(native_event);
  EXPECT_FALSE(uploader.IsSessionDataUploadAllowed());
}
#endif

TEST_F(StatusUploaderTest, NoUploadAfterVideoCapture) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_);
  // Should allow data upload before there is video capture.
  EXPECT_TRUE(uploader.IsSessionDataUploadAllowed());

  // Now mock video capture, and no session data should be allowed.
  MediaCaptureDevicesDispatcher::GetInstance()->OnMediaRequestStateChanged(
      0, 0, 0, GURL("http://www.google.com"),
      content::MEDIA_DEVICE_VIDEO_CAPTURE,
      content::MEDIA_REQUEST_STATE_OPENING);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(uploader.IsSessionDataUploadAllowed());
}

}  // namespace policy
