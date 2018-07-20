// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_manager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/power/ml/fake_boot_clock.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/smart_dim_model.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/session_manager_types.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace chromeos {
namespace power {
namespace ml {

using content::WebContentsTester;

void EqualEvent(const UserActivityEvent::Event& expected_event,
                const UserActivityEvent::Event& result_event) {
  EXPECT_EQ(expected_event.type(), result_event.type());
  EXPECT_EQ(expected_event.reason(), result_event.reason());
  EXPECT_EQ(expected_event.log_duration_sec(), result_event.log_duration_sec());
  EXPECT_EQ(expected_event.screen_dim_occurred(),
            result_event.screen_dim_occurred());
  EXPECT_EQ(expected_event.screen_off_occurred(),
            result_event.screen_off_occurred());
  EXPECT_EQ(expected_event.screen_lock_occurred(),
            result_event.screen_lock_occurred());
}

void EqualModelPrediction(
    const UserActivityEvent::ModelPrediction& expected_prediction,
    const UserActivityEvent::ModelPrediction& result_prediction) {
  EXPECT_EQ(expected_prediction.decision_threshold(),
            result_prediction.decision_threshold());
  EXPECT_EQ(expected_prediction.inactivity_score(),
            result_prediction.inactivity_score());
  EXPECT_EQ(expected_prediction.model_applied(),
            result_prediction.model_applied());
}

// Testing UKM logger.
class TestingUserActivityUkmLogger : public UserActivityUkmLogger {
 public:
  TestingUserActivityUkmLogger() = default;
  ~TestingUserActivityUkmLogger() override = default;

  const std::vector<UserActivityEvent>& events() const { return events_; }

  // UserActivityUkmLogger overrides:
  void LogActivity(const UserActivityEvent& event) override {
    events_.push_back(event);
  }

 private:
  std::vector<UserActivityEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(TestingUserActivityUkmLogger);
};

// Testing smart dim model.
class FakeSmartDimModel : public SmartDimModel {
 public:
  FakeSmartDimModel() = default;
  ~FakeSmartDimModel() override = default;

  void set_inactive_probability(const float inactive_probability) {
    inactive_probability_ = inactive_probability;
  }

  void set_threshold(const float threshold) { threshold_ = threshold; }

  // SmartDimModel overrides:
  bool ShouldDim(const UserActivityEvent::Features& features,
                 float* inactive_probability_out,
                 float* threshold_out) override {
    DCHECK(inactive_probability_out);
    DCHECK(threshold_out);
    *inactive_probability_out = inactive_probability_;
    *threshold_out = threshold_;
    return inactive_probability_ >= threshold_;
  }

 private:
  float inactive_probability_ = 0;
  float threshold_ = 0;
  DISALLOW_COPY_AND_ASSIGN(FakeSmartDimModel);
};

class UserActivityManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  UserActivityManagerTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {
    fake_power_manager_client_.Init(nullptr);
    viz::mojom::VideoDetectorObserverPtr observer;
    idle_event_notifier_ = std::make_unique<IdleEventNotifier>(
        &fake_power_manager_client_, &user_activity_detector_,
        mojo::MakeRequest(&observer));
    activity_logger_ = std::make_unique<UserActivityManager>(
        &delegate_, idle_event_notifier_.get(), &user_activity_detector_,
        &fake_power_manager_client_, &session_manager_,
        mojo::MakeRequest(&observer), &fake_user_manager_, &model_);
    activity_logger_->SetTaskRunnerForTesting(
        task_runner_, std::make_unique<FakeBootClock>(
                          task_runner_, base::TimeDelta::FromSeconds(10)));
  }

  ~UserActivityManagerTest() override = default;

 protected:
  void ReportUserActivity(const ui::Event* event) {
    activity_logger_->OnUserActivity(event);
  }

  void ReportIdleEvent(const IdleEventNotifier::ActivityData& data) {
    activity_logger_->OnIdleEventObserved(data);
  }

  void ReportLidEvent(chromeos::PowerManagerClient::LidState state) {
    fake_power_manager_client_.SetLidState(state, base::TimeTicks::UnixEpoch());
  }

  void ReportPowerChangeEvent(
      power_manager::PowerSupplyProperties::ExternalPower power,
      float battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_external_power(power);
    proto.set_battery_percent(battery_percent);
    fake_power_manager_client_.UpdatePowerProperties(proto);
  }

  void ReportTabletModeEvent(chromeos::PowerManagerClient::TabletMode mode) {
    fake_power_manager_client_.SetTabletMode(mode,
                                             base::TimeTicks::UnixEpoch());
  }

  void ReportVideoStart() { activity_logger_->OnVideoActivityStarted(); }

  void ReportScreenIdleState(bool screen_dim, bool screen_off) {
    power_manager::ScreenIdleState proto;
    proto.set_dimmed(screen_dim);
    proto.set_off(screen_off);
    fake_power_manager_client_.SendScreenIdleStateChanged(proto);
  }

  void ReportScreenLocked() {
    session_manager_.SetSessionState(session_manager::SessionState::LOCKED);
  }

  void ReportSuspend(power_manager::SuspendImminent::Reason reason,
                     base::TimeDelta sleep_duration) {
    fake_power_manager_client_.SendSuspendImminent(reason);
    GetTaskRunner()->FastForwardBy(sleep_duration);
    fake_power_manager_client_.SendSuspendDone(sleep_duration);
  }

  void ReportInactivityDelays(base::TimeDelta screen_dim_delay,
                              base::TimeDelta screen_off_delay) {
    power_manager::PowerManagementPolicy::Delays proto;
    proto.set_screen_dim_ms(screen_dim_delay.InMilliseconds());
    proto.set_screen_off_ms(screen_off_delay.InMilliseconds());
    fake_power_manager_client_.SetInactivityDelays(proto);
  }

  int GetNumberOfDeferredDims() {
    return fake_power_manager_client_.num_defer_screen_dim_calls();
  }

  TabProperty UpdateOpenTabURL() {
    return activity_logger_->UpdateOpenTabURL();
  }

  // Creates a test browser window and sets its visibility, activity and
  // incognito status.
  std::unique_ptr<Browser> CreateTestBrowser(bool is_visible,
                                             bool is_focused,
                                             bool is_incognito = false) {
    Profile* const original_profile = profile();
    Profile* const used_profile =
        is_incognito ? original_profile->GetOffTheRecordProfile()
                     : original_profile;
    Browser::CreateParams params(used_profile, true);

    auto dummy_window = std::make_unique<aura::Window>(nullptr);
    dummy_window->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(dummy_window.get());
    dummy_window->SetBounds(gfx::Rect(root_window()->bounds().size()));
    if (is_visible) {
      dummy_window->Show();
    } else {
      dummy_window->Hide();
    }

    std::unique_ptr<Browser> browser =
        chrome::CreateBrowserWithAuraTestWindowForParams(
            std::move(dummy_window), &params);
    if (is_focused) {
      browser->window()->Activate();
    } else {
      browser->window()->Deactivate();
    }
    return browser;
  }

  // Adds a tab with specified url to the tab strip model. Also optionally sets
  // the tab to be the active one in the tab strip model.
  // If |mime_type| is an empty string, the content has a default text type.
  // TODO(jiameng): there doesn't seem to be a way to set form entry (via
  // page importance signal). Check if there's some other way to set it.
  ukm::SourceId CreateTestWebContents(TabStripModel* const tab_strip_model,
                                      const GURL& url,
                                      bool is_active,
                                      const std::string& mime_type = "") {
    DCHECK(tab_strip_model);
    DCHECK(!url.is_empty());
    content::WebContents* contents =
        tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model, url);
    if (is_active) {
      tab_strip_model->ActivateTabAt(tab_strip_model->count() - 1, false);
    }
    if (!mime_type.empty())
      WebContentsTester::For(contents)->SetMainFrameMimeType(mime_type);

    WebContentsTester::For(contents)->TestSetIsLoading(false);
    return ukm::GetSourceIdForWebContentsDocument(contents);
  }

  const scoped_refptr<base::TestMockTimeTaskRunner>& GetTaskRunner() {
    return task_runner_;
  }

  TestingUserActivityUkmLogger delegate_;
  FakeSmartDimModel model_;
  chromeos::FakeChromeUserManager fake_user_manager_;
  // Only used to get SourceIds for URLs.
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  TabActivitySimulator tab_activity_simulator_;

  const GURL url1_ = GURL("https://example1.com/");
  const GURL url2_ = GURL("https://example2.com/");
  const GURL url3_ = GURL("https://example3.com/");
  const GURL url4_ = GURL("https://example4.com/");

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  ui::UserActivityDetector user_activity_detector_;
  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  chromeos::FakePowerManagerClient fake_power_manager_client_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<UserActivityManager> activity_logger_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityManagerTest);
};

// After an idle event, we have a ui::Event, we should expect one
// UserActivityEvent.
TEST_F(UserActivityManagerTest, LogAfterIdleEvent) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(2);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
  EXPECT_FALSE(events[0].has_model_prediction());
  EXPECT_EQ(0, events[0].features().previous_positive_actions_count());
  EXPECT_EQ(0, events[0].features().previous_negative_actions_count());
}

// Get a user event before an idle event, we should not log it.
TEST_F(UserActivityManagerTest, LogBeforeIdleEvent) {
  ReportUserActivity(nullptr);
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  EXPECT_EQ(0U, delegate_.events().size());
}

// Get a user event, then an idle event, then another user event,
// we should log the last one.
TEST_F(UserActivityManagerTest, LogSecondEvent) {
  ReportUserActivity(nullptr);
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  // Another user event.
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
  EXPECT_FALSE(events[0].has_model_prediction());
  EXPECT_EQ(0, events[0].features().previous_positive_actions_count());
  EXPECT_EQ(0, events[0].features().previous_negative_actions_count());
}

// Log multiple events.
TEST_F(UserActivityManagerTest, LogMultipleEvents) {
  // Trigger the 1st idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  // First user event.
  ReportUserActivity(nullptr);

  // Trigger the 2nd idle event.
  ReportIdleEvent(data);
  // Second user event.
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ReportUserActivity(nullptr);

  // Trigger the 3rd idle event.
  ReportIdleEvent(data);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(3));
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(10));

  // Trigger the 4th idle event.
  ReportIdleEvent(data);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(4));
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(10));

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(4U, events.size());

  UserActivityEvent::Event expected_event1;
  expected_event1.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event1.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event1.set_log_duration_sec(0);
  expected_event1.set_screen_dim_occurred(false);
  expected_event1.set_screen_off_occurred(false);
  expected_event1.set_screen_lock_occurred(false);

  UserActivityEvent::Event expected_event2;
  expected_event2.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event2.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event2.set_log_duration_sec(2);
  expected_event2.set_screen_dim_occurred(false);
  expected_event2.set_screen_off_occurred(false);
  expected_event2.set_screen_lock_occurred(false);

  UserActivityEvent::Event expected_event3;
  expected_event3.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event3.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event3.set_log_duration_sec(3);
  expected_event3.set_screen_dim_occurred(false);
  expected_event3.set_screen_off_occurred(false);
  expected_event3.set_screen_lock_occurred(false);

  UserActivityEvent::Event expected_event4;
  expected_event4.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event4.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event4.set_log_duration_sec(4);
  expected_event4.set_screen_dim_occurred(false);
  expected_event4.set_screen_off_occurred(false);
  expected_event4.set_screen_lock_occurred(false);

  EqualEvent(expected_event1, events[0].event());
  EqualEvent(expected_event2, events[1].event());
  EqualEvent(expected_event3, events[2].event());
  EqualEvent(expected_event4, events[3].event());
  EXPECT_FALSE(events[0].has_model_prediction());
  EXPECT_FALSE(events[1].has_model_prediction());
  EXPECT_FALSE(events[2].has_model_prediction());
  EXPECT_FALSE(events[3].has_model_prediction());

  EXPECT_EQ(0, events[0].features().previous_positive_actions_count());
  EXPECT_EQ(0, events[0].features().previous_negative_actions_count());

  EXPECT_EQ(0, events[1].features().previous_positive_actions_count());
  EXPECT_EQ(1, events[1].features().previous_negative_actions_count());

  EXPECT_EQ(0, events[2].features().previous_positive_actions_count());
  EXPECT_EQ(2, events[2].features().previous_negative_actions_count());

  EXPECT_EQ(1, events[3].features().previous_positive_actions_count());
  EXPECT_EQ(2, events[3].features().previous_negative_actions_count());
}

TEST_F(UserActivityManagerTest, UserCloseLid) {
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ReportLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  const std::vector<UserActivityEvent>& events = delegate_.events();
  EXPECT_TRUE(events.empty());
}

TEST_F(UserActivityManagerTest, PowerChangeActivity) {
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  // We don't care about battery percentage change, but only power source.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 25.0f);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         28.0f);
  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::POWER_CHANGED);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityManagerTest, VideoActivity) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportVideoStart();
  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::VIDEO_ACTIVITY);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

// System remains idle, screen is dimmed then turned off, and system is finally
// suspended.
TEST_F(UserActivityManagerTest, SystemIdleSuspend) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportScreenIdleState(true /* screen_dim */, false /* screen_off */);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(30));
  ReportScreenIdleState(true /* screen_dim */, true /* screen_off */);
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(10));

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event.set_log_duration_sec(50);
  expected_event.set_screen_dim_occurred(true);
  expected_event.set_screen_off_occurred(true);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

// System remains idle, screen is dimmed then turned off, but system is not
// suspended.
TEST_F(UserActivityManagerTest, SystemIdleNotSuspend) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportScreenIdleState(true /* screen_dim */, false /* screen_off */);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(30));
  ReportScreenIdleState(true /* screen_dim */, true /* screen_off */);
  GetTaskRunner()->FastForwardUntilNoTasksRemain();

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(0U, events.size());
}

// Test system idle interrupt by user activity.
// We should only observe user activity.
TEST_F(UserActivityManagerTest, SystemIdleInterrupted) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportScreenIdleState(true /* screen_dim */, false /* screen_off */);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(30));
  ReportScreenIdleState(true /* screen_dim */, true /* screen_off */);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  ReportUserActivity(nullptr);
  GetTaskRunner()->FastForwardUntilNoTasksRemain();

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(51);
  expected_event.set_screen_dim_occurred(true);
  expected_event.set_screen_off_occurred(true);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityManagerTest, ScreenLockNoSuspend) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportScreenLocked();
  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(0U, events.size());
}

TEST_F(UserActivityManagerTest, ScreenLockWithSuspend) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportScreenLocked();
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(1));

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(true);
  EqualEvent(expected_event, events[0].event());
}

// As we log when SuspendImminent is received, sleep duration from SuspendDone
// doesn't make any difference.
TEST_F(UserActivityManagerTest, SuspendIdleShortSleepDuration) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(1));
  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event.set_log_duration_sec(20);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityManagerTest, SuspendLidClosed) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportSuspend(power_manager::SuspendImminent_Reason_LID_CLOSED,
                base::TimeDelta::FromSeconds(10));
  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::OFF);
  expected_event.set_reason(UserActivityEvent::Event::LID_CLOSED);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityManagerTest, SuspendOther) {
  // Trigger an idle event.
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportSuspend(power_manager::SuspendImminent_Reason_OTHER,
                base::TimeDelta::FromSeconds(10));
  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::OFF);
  expected_event.set_reason(UserActivityEvent::Event::MANUAL_SLEEP);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

// Test feature extraction.
TEST_F(UserActivityManagerTest, FeatureExtraction) {
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = UserActivityEvent_Features_DayOfWeek_MON;
  data.last_activity_time_of_day = base::TimeDelta::FromSeconds(100);
  data.recent_time_active = base::TimeDelta::FromSeconds(10);
  data.time_since_last_mouse = base::TimeDelta::FromSeconds(20);
  data.time_since_last_touch = base::TimeDelta::FromSeconds(30);
  data.video_playing_time = base::TimeDelta::FromSeconds(90);
  data.time_since_video_ended = base::TimeDelta::FromSeconds(2);
  data.key_events_in_last_hour = 0;
  data.mouse_events_in_last_hour = 10;
  data.touch_events_in_last_hour = 20;

  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(UserActivityEvent::Features::CLAMSHELL, features.device_mode());
  EXPECT_EQ(23.0f, features.battery_percent());
  EXPECT_FALSE(features.on_battery());
  EXPECT_EQ(UserActivityEvent::Features::UNMANAGED,
            features.device_management());
  EXPECT_EQ(UserActivityEvent_Features_DayOfWeek_MON,
            features.last_activity_day());
  EXPECT_EQ(100, features.last_activity_time_sec());
  EXPECT_EQ(10, features.recent_time_active_sec());
  EXPECT_EQ(20, features.time_since_last_mouse_sec());
  EXPECT_EQ(30, features.time_since_last_touch_sec());
  EXPECT_EQ(90, features.video_playing_time_sec());
  EXPECT_EQ(2, features.time_since_video_ended_sec());
  EXPECT_EQ(0, features.key_events_in_last_hour());
  EXPECT_EQ(10, features.mouse_events_in_last_hour());
  EXPECT_EQ(20, features.touch_events_in_last_hour());
  EXPECT_FALSE(features.has_last_user_activity_time_sec());
  EXPECT_FALSE(features.has_time_since_last_key_sec());
  EXPECT_FALSE(features.screen_dimmed_initially());
  EXPECT_FALSE(features.screen_off_initially());
  EXPECT_FALSE(features.screen_locked_initially());
}

TEST_F(UserActivityManagerTest, ManagedDevice) {
  fake_user_manager_.set_is_enterprise_managed(true);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(UserActivityEvent::Features::MANAGED, features.device_management());
}

TEST_F(UserActivityManagerTest, DimAndOffDelays) {
  ReportInactivityDelays(
      base::TimeDelta::FromMilliseconds(2000) /* screen_dim_delay */,
      base::TimeDelta::FromMilliseconds(3000) /* screen_off_delay */);
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(2, features.on_to_dim_sec());
  EXPECT_EQ(1, features.dim_to_screen_off_sec());
}

TEST_F(UserActivityManagerTest, DimDelays) {
  ReportInactivityDelays(
      base::TimeDelta::FromMilliseconds(2000) /* screen_dim_delay */,
      base::TimeDelta() /* screen_off_delay */);
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(2, features.on_to_dim_sec());
  EXPECT_TRUE(!features.has_dim_to_screen_off_sec());
}

TEST_F(UserActivityManagerTest, OffDelays) {
  ReportInactivityDelays(
      base::TimeDelta() /* screen_dim_delay */,
      base::TimeDelta::FromMilliseconds(4000) /* screen_off_delay */);
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(4, features.dim_to_screen_off_sec());
  EXPECT_TRUE(!features.has_on_to_dim_sec());
}

// Screen is off when idle event is reported. No subsequent change in screen
// state.
TEST_F(UserActivityManagerTest, InitialScreenOff) {
  ReportScreenIdleState(true /* screen_dim */, true /* screen_off */);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportScreenIdleState(false /* screen_dim */, true /* screen_off */);

  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(7));
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_TRUE(features.screen_dimmed_initially());
  EXPECT_TRUE(features.screen_off_initially());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(7);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

// Screen is off when idle event is reported. No subsequent change in screen
// state.
TEST_F(UserActivityManagerTest, InitialScreenStateFlipped) {
  ReportScreenIdleState(true /* screen_dim */, false /* screen_off */);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportScreenIdleState(false /* screen_dim */, false /* screen_off */);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(7));
  ReportScreenIdleState(true /* screen_dim */, true /* screen_off */);

  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_TRUE(features.screen_dimmed_initially());
  EXPECT_FALSE(features.screen_off_initially());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(7);
  expected_event.set_screen_dim_occurred(true);
  expected_event.set_screen_off_occurred(true);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

// Screen is off when idle event is reported. No subsequent change in screen
// state.
TEST_F(UserActivityManagerTest, ScreenOffStateChanged) {
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);

  ReportScreenIdleState(true /* screen_dim */, false /* screen_off */);
  ReportScreenIdleState(true /* screen_dim */, true /* screen_off */);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(7));
  ReportScreenIdleState(false /* screen_dim */, false /* screen_off */);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_FALSE(features.screen_dimmed_initially());
  EXPECT_FALSE(features.screen_off_initially());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(7);
  expected_event.set_screen_dim_occurred(true);
  expected_event.set_screen_off_occurred(true);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityManagerTest, ScreenDimDeferredWithFinalEvent) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", "0.651"}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);

  model_.set_inactive_probability(0.6);
  model_.set_threshold(0.651);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);
  EXPECT_EQ(1, GetNumberOfDeferredDims());

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  expected_event.set_log_duration_sec(0);
  expected_event.set_screen_dim_occurred(false);
  expected_event.set_screen_off_occurred(false);
  expected_event.set_screen_lock_occurred(false);
  EqualEvent(expected_event, events[0].event());

  UserActivityEvent::ModelPrediction expected_prediction;
  expected_prediction.set_decision_threshold(65);
  expected_prediction.set_inactivity_score(60);
  expected_prediction.set_model_applied(true);
  EqualModelPrediction(expected_prediction, events[0].model_prediction());
}

TEST_F(UserActivityManagerTest, ScreenDimDeferredWithoutFinalEvent) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", "0.651"}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);

  model_.set_inactive_probability(0.6);
  model_.set_threshold(0.651);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  EXPECT_EQ(1, GetNumberOfDeferredDims());

  const std::vector<UserActivityEvent>& events = delegate_.events();
  EXPECT_TRUE(events.empty());
}

TEST_F(UserActivityManagerTest, ScreenDimNotDeferred) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", base::NumberToString(0.5)}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);

  model_.set_inactive_probability(0.6);
  model_.set_threshold(0.5);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);
  EXPECT_EQ(0, GetNumberOfDeferredDims());

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::ModelPrediction expected_prediction;
  expected_prediction.set_decision_threshold(50);
  expected_prediction.set_inactivity_score(60);
  expected_prediction.set_model_applied(true);
  EqualModelPrediction(expected_prediction, events[0].model_prediction());
}

TEST_F(UserActivityManagerTest, TwoScreenDimImminentWithEventInBetween) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", base::NumberToString(0.5)}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);
  model_.set_threshold(0.5);

  // 1st ScreenDimImminent gets deferred
  model_.set_inactive_probability(0.4);

  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  EXPECT_EQ(1, GetNumberOfDeferredDims());

  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(6));
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(3));

  // 2nd ScreenDimImminent is not deferred despite model score says so.
  model_.set_inactive_probability(0.2);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEvent(data);
  EXPECT_EQ(1, GetNumberOfDeferredDims());

  // Log when a SuspendImminent is received
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(3));

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(2U, events.size());

  // The first screen dim imminent event.
  UserActivityEvent::Event expected_event1;
  expected_event1.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event1.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event1.set_log_duration_sec(6);
  expected_event1.set_screen_dim_occurred(false);
  expected_event1.set_screen_off_occurred(false);
  expected_event1.set_screen_lock_occurred(false);
  EqualEvent(expected_event1, events[0].event());

  UserActivityEvent::ModelPrediction expected_prediction1;
  expected_prediction1.set_decision_threshold(50);
  expected_prediction1.set_inactivity_score(40);
  expected_prediction1.set_model_applied(true);
  EqualModelPrediction(expected_prediction1, events[0].model_prediction());

  // The second screen dim imminent event.
  UserActivityEvent::Event expected_event2;
  expected_event2.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event2.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event2.set_log_duration_sec(20);
  expected_event2.set_screen_dim_occurred(false);
  expected_event2.set_screen_off_occurred(false);
  expected_event2.set_screen_lock_occurred(false);
  EqualEvent(expected_event2, events[1].event());

  UserActivityEvent::ModelPrediction expected_prediction2;
  expected_prediction2.set_decision_threshold(50);
  expected_prediction2.set_inactivity_score(20);
  expected_prediction2.set_model_applied(false);
  EqualModelPrediction(expected_prediction2, events[1].model_prediction());
}

TEST_F(UserActivityManagerTest, TwoScreenDimImminentWithoutEventInBetween) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", base::NumberToString(0.5)}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUserActivityPrediction, params);
  model_.set_threshold(0.5);

  // 1st ScreenDimImminent gets deferred
  model_.set_inactive_probability(0.4);
  const IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  EXPECT_EQ(1, GetNumberOfDeferredDims());

  // 2nd ScreenDimImminent is not deferred despite model score says so.
  model_.set_inactive_probability(0.2);
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(10));
  ReportIdleEvent(data);
  EXPECT_EQ(1, GetNumberOfDeferredDims());

  // Log when a SuspendImminent is received
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(20));
  ReportSuspend(power_manager::SuspendImminent_Reason_IDLE,
                base::TimeDelta::FromSeconds(3));

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(2U, events.size());

  // The first event logged is the current one.
  UserActivityEvent::Event expected_event1;
  expected_event1.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event1.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  expected_event1.set_log_duration_sec(20);
  expected_event1.set_screen_dim_occurred(false);
  expected_event1.set_screen_off_occurred(false);
  expected_event1.set_screen_lock_occurred(false);
  EqualEvent(expected_event1, events[0].event());

  UserActivityEvent::ModelPrediction expected_prediction1;
  expected_prediction1.set_decision_threshold(50);
  expected_prediction1.set_inactivity_score(20);
  expected_prediction1.set_model_applied(false);
  EqualModelPrediction(expected_prediction1, events[0].model_prediction());

  // The earlier idle event is logged afterwards.
  UserActivityEvent::Event expected_event2 = expected_event1;
  expected_event2.set_log_duration_sec(30);
  EqualEvent(expected_event2, events[1].event());

  UserActivityEvent::ModelPrediction expected_prediction2;
  expected_prediction2.set_decision_threshold(50);
  expected_prediction2.set_inactivity_score(40);
  expected_prediction2.set_model_applied(true);
  EqualModelPrediction(expected_prediction2, events[1].model_prediction());
}

TEST_F(UserActivityManagerTest, BasicTabs) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  BrowserList::GetInstance()->SetLastActive(browser.get());
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  const ukm::SourceId source_id1 = CreateTestWebContents(
      tab_strip_model, url1_, true /* is_active */, "application/pdf");
  SiteEngagementService::Get(profile())->ResetBaseScoreForURL(url1_, 95);

  CreateTestWebContents(tab_strip_model, url2_, false /* is_active */);

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(features.source_id(), source_id1);
  EXPECT_EQ(features.tab_domain(), url1_.host());
  EXPECT_FALSE(features.tab_domain().empty());
  EXPECT_EQ(features.engagement_score(), 90);
  EXPECT_FALSE(features.has_form_entry());

  tab_strip_model->CloseAllTabs();
}

TEST_F(UserActivityManagerTest, MultiBrowsersAndTabs) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 and browser3 are both visible but browser2 is the topmost.
  std::unique_ptr<Browser> browser1 =
      CreateTestBrowser(false /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser2 =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  std::unique_ptr<Browser> browser3 =
      CreateTestBrowser(true /* is_visible */, false /* is_focused */);

  BrowserList::GetInstance()->SetLastActive(browser3.get());
  BrowserList::GetInstance()->SetLastActive(browser2.get());
  BrowserList::GetInstance()->SetLastActive(browser1.get());

  TabStripModel* tab_strip_model1 = browser1->tab_strip_model();
  CreateTestWebContents(tab_strip_model1, url1_, false /* is_active */);
  CreateTestWebContents(tab_strip_model1, url2_, true /* is_active */);

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  const ukm::SourceId source_id3 =
      CreateTestWebContents(tab_strip_model2, url3_, true /* is_active */);

  TabStripModel* tab_strip_model3 = browser3->tab_strip_model();
  CreateTestWebContents(tab_strip_model3, url4_, true /* is_active */);

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(features.source_id(), source_id3);
  EXPECT_EQ(features.tab_domain(), url3_.host());
  EXPECT_EQ(features.engagement_score(), 0);
  EXPECT_FALSE(features.has_form_entry());

  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
  tab_strip_model3->CloseAllTabs();
}

TEST_F(UserActivityManagerTest, Incognito) {
  std::unique_ptr<Browser> browser = CreateTestBrowser(
      true /* is_visible */, true /* is_focused */, true /* is_incognito */);
  BrowserList::GetInstance()->SetLastActive(browser.get());

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CreateTestWebContents(tab_strip_model, url1_, true /* is_active */);
  CreateTestWebContents(tab_strip_model, url2_, false /* is_active */);

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_FALSE(features.has_source_id());
  EXPECT_FALSE(features.has_tab_domain());
  EXPECT_FALSE(features.has_engagement_score());
  EXPECT_FALSE(features.has_has_form_entry());

  tab_strip_model->CloseAllTabs();
}

TEST_F(UserActivityManagerTest, NoOpenTabs) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);

  IdleEventNotifier::ActivityData data;
  ReportIdleEvent(data);
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = delegate_.events();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_FALSE(features.has_source_id());
  EXPECT_FALSE(features.has_tab_domain());
  EXPECT_FALSE(features.has_engagement_score());
  EXPECT_FALSE(features.has_has_form_entry());
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
