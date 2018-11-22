// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/adapter.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_als_reader.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_brightness_monitor.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// Testing modeller.
class FakeModeller : public Modeller {
 public:
  FakeModeller() = default;
  ~FakeModeller() override = default;

  void InitModellerWithCurves(
      const base::Optional<MonotoneCubicSpline>& global_curve,
      const base::Optional<MonotoneCubicSpline>& personal_curve) {
    DCHECK(!modeller_initialized_);
    modeller_initialized_ = true;
    if (global_curve)
      global_curve_.emplace(*global_curve);

    if (personal_curve)
      personal_curve_.emplace(*personal_curve);
  }

  void ReportModelTrained(const MonotoneCubicSpline& personal_curve) {
    DCHECK(modeller_initialized_);
    personal_curve_.emplace(personal_curve);
    for (auto& observer : observers_)
      observer.OnModelTrained(personal_curve);
  }

  void ReportModelInitialized() {
    DCHECK(modeller_initialized_);
    for (auto& observer : observers_)
      observer.OnModelInitialized(global_curve_, personal_curve_);
  }

  // Modeller overrides:
  void AddObserver(Modeller::Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
    if (modeller_initialized_)
      observer->OnModelInitialized(global_curve_, personal_curve_);
  }

  void RemoveObserver(Modeller::Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

 private:
  bool modeller_initialized_ = false;
  base::Optional<MonotoneCubicSpline> global_curve_;
  base::Optional<MonotoneCubicSpline> personal_curve_;

  base::ObserverList<Observer> observers_;
};

class TestObserver : public PowerManagerClient::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // chromeos::PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override {
    ++num_changes_;
    change_ = change;
  }
  double GetBrightnessPercent() const { return change_.percent(); }

  int num_changes() const { return num_changes_; }

  power_manager::BacklightBrightnessChange_Cause GetCause() const {
    return change_.cause();
  }

 private:
  int num_changes_ = 0;
  power_manager::BacklightBrightnessChange change_;
};

}  // namespace

class AdapterTest : public testing::Test {
 public:
  AdapterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        thread_bundle_(content::TestBrowserThreadBundle::PLAIN_MAINLOOP) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        std::make_unique<chromeos::FakePowerManagerClient>());
    power_manager::SetBacklightBrightnessRequest request;
    request.set_percent(1);
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->SetScreenBrightness(request);
    scoped_task_environment_.RunUntilIdle();

    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
        &test_observer_);

    global_curve_.emplace(MonotoneCubicSpline({-4, 12, 20}, {30, 80, 100}));
    personal_curve_.emplace(MonotoneCubicSpline({-4, 12, 20}, {20, 60, 100}));
  }

  ~AdapterTest() override {
    base::TaskScheduler::GetInstance()->FlushForTesting();
  }

  void SetUpAdapter(const std::map<std::string, std::string>& params,
                    bool brightness_set_by_policy = false) {
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_user_prefs(base::WrapRefCounted(new TestingPrefStore()));
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);

    chromeos::power::auto_screen_brightness::MetricsReporter::
        RegisterLocalStatePrefs(registry.get());

    // Same default values as used in the actual pref store.
    registry->RegisterIntegerPref(ash::prefs::kPowerAcScreenBrightnessPercent,
                                  -1, PrefRegistry::PUBLIC);
    registry->RegisterIntegerPref(
        ash::prefs::kPowerBatteryScreenBrightnessPercent, -1,
        PrefRegistry::PUBLIC);

    sync_preferences::PrefServiceSyncable* regular_prefs =
        factory.CreateSyncable(registry.get()).release();

    RegisterUserProfilePrefs(registry.get());
    if (brightness_set_by_policy) {
      regular_prefs->SetInteger(ash::prefs::kPowerAcScreenBrightnessPercent,
                                10);
      regular_prefs->SetInteger(
          ash::prefs::kPowerBatteryScreenBrightnessPercent, 10);
    }

    CHECK(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_builder.SetPrefService(base::WrapUnique(regular_prefs));

    profile_ = profile_builder.Build();

    base::test::ScopedFeatureList scoped_feature_list;
    if (!params.empty()) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kAutoScreenBrightness, params);
    }

    adapter_ = std::make_unique<Adapter>(
        profile_.get(), &fake_als_reader_, &fake_brightness_monitor_,
        &fake_modeller_, nullptr /* metrics_reporter */,
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
    adapter_->SetTickClockForTesting(
        scoped_task_environment_.GetMockTickClock());
  }

  void Init(AlsReader::AlsInitStatus als_reader_status,
            BrightnessMonitor::Status brightness_monitor_status,
            const base::Optional<MonotoneCubicSpline>& global_curve,
            const base::Optional<MonotoneCubicSpline>& personal_curve,
            const std::map<std::string, std::string>& params,
            bool brightness_set_by_policy = false) {
    fake_als_reader_.set_als_init_status(als_reader_status);
    fake_brightness_monitor_.set_status(brightness_monitor_status);
    fake_modeller_.InitModellerWithCurves(global_curve, personal_curve);
    SetUpAdapter(params, brightness_set_by_policy);
    scoped_task_environment_.RunUntilIdle();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle thread_bundle_;

  TestObserver test_observer_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;

  base::Optional<MonotoneCubicSpline> global_curve_;
  base::Optional<MonotoneCubicSpline> personal_curve_;

  FakeAlsReader fake_als_reader_;
  FakeBrightnessMonitor fake_brightness_monitor_;
  FakeModeller fake_modeller_;

  const std::map<std::string, std::string> default_params_ = {
      {"brightening_lux_threshold_ratio", "0.1"},
      {"darkening_lux_threshold_ratio", "0.2"},
      {"immediate_brightening_lux_threshold_ratio", "3"},
      {"immediate_darkening_lux_threshold_ratio", "1"},
      {"update_brightness_on_startup", "true"},
      {"min_seconds_between_brightness_changes", "10"},
      {"model_curve", "2"},
  };

  std::unique_ptr<Adapter> adapter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AdapterTest);
};

// AlsReader is |kDisabled| when Adapter is created.
TEST_F(AdapterTest, AlsReaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kDisabled, BrightnessMonitor::Status::kSuccess,
       global_curve_, base::nullopt /* personal_curve */, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// BrightnessMonitor is |kDisabled| when Adapter is created.
TEST_F(AdapterTest, BrightnessMonitorDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kDisabled,
       global_curve_, base::nullopt /* personal_curve */, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// Modeller is |kDisabled| when Adapter is created.
TEST_F(AdapterTest, ModellerDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       base::nullopt /* global_curve */, base::nullopt /* personal_curve */,
       default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// AlsReader is |kDisabled| on later notification.
TEST_F(AdapterTest, AlsReaderDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess, global_curve_,
       base::nullopt /* personal_curve */, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kDisabled);
  fake_als_reader_.ReportReaderInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// AlsReader is |kSuccess| on later notification.
TEST_F(AdapterTest, AlsReaderEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess, global_curve_,
       base::nullopt /* personal_curve */, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_als_reader_.ReportReaderInitialized();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// BrightnessMonitor is |kDisabled| on later notification.
TEST_F(AdapterTest, BrightnessMonitorDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing, global_curve_,
       base::nullopt /* personal_curve */, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kDisabled);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

// BrightnessMonitor is |kSuccess| on later notification.
TEST_F(AdapterTest, BrightnessMonitorEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing, global_curve_,
       base::nullopt /* personal_curve */, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// Modeller is |kDisabled| on later notification.
TEST_F(AdapterTest, ModellerDisabledOnNotification) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpAdapter(default_params_);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_modeller_.InitModellerWithCurves(base::nullopt, base::nullopt);
  fake_modeller_.ReportModelInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
  EXPECT_FALSE(adapter_->GetGlobalCurveForTesting());
  EXPECT_FALSE(adapter_->GetPersonalCurveForTesting());
}

// Modeller is |kSuccess| on later notification.
TEST_F(AdapterTest, ModellerEnabledOnNotification) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpAdapter(default_params_);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kInitializing);

  fake_modeller_.InitModellerWithCurves(global_curve_, personal_curve_);
  fake_modeller_.ReportModelInitialized();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);
}

TEST_F(AdapterTest, SequenceOfBrightnessUpdatesWithDefaultParams) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  // Forward by 1sec because in real implementation, |first_als_time_| is zero
  // if there's no ALS reading received.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Brightness is changed after the 1st ALS reading comes in.
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);

  // Another ALS value is received in 3 sec, but no brightness update is done.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(3));
  fake_als_reader_.ReportAmbientLightUpdate(20);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);

  // |params.min_time_between_brightness_changes| has elapsed since we've made
  // the change, but there's no new ALS value, hence no brightness change is
  // triggered.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(test_observer_.num_changes(), 1);

  // A new ALS value triggers a brightness change.
  fake_als_reader_.ReportAmbientLightUpdate(40);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 2);

  // Adapter is disabled after a user manual adjustment.
  fake_brightness_monitor_.ReportUserBrightnessChangeRequested();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);

  // Another user manual adjustment came in.
  fake_brightness_monitor_.ReportUserBrightnessChangeRequested();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);

}

TEST_F(AdapterTest, UserBrightnessRequestBeforeAnyModelUpdate) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  // Adapter is disabled after a user manual adjustment.
  fake_brightness_monitor_.ReportUserBrightnessChangeRequested();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);

  // Another user manual adjustment came in.
  fake_brightness_monitor_.ReportUserBrightnessChangeRequested();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
}

TEST_F(AdapterTest, BrightnessLuxThresholds) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, default_params_);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Brightness is changed after the 1st ALS value, and the thresholds are
  // changed.
  fake_als_reader_.ReportAmbientLightUpdate(20);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(), 20);
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(), 22);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(), 16);

  // A 2nd ALS comes in, but average ambient is within the thresholds, hence
  // brightness isn't changed and thresholds aren't updated.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  fake_als_reader_.ReportAmbientLightUpdate(21);
  EXPECT_EQ(1, test_observer_.num_changes());
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(), (20 + 21) / 2.0);
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(), 22);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(), 16);

  // A 3rd ALS comes in, but still not enough to trigger brightness change.
  fake_als_reader_.ReportAmbientLightUpdate(15);
  EXPECT_EQ(test_observer_.num_changes(), 1);
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(), (21 + 15) / 2.0);
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(), 22);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(), 16);

  // A 4th ALS makes average value below the darkening threshold, hence
  // brightness is changed. Thresholds are also changed.
  fake_als_reader_.ReportAmbientLightUpdate(7);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(test_observer_.num_changes(), 2);
  EXPECT_EQ(Adapter::kNumberAmbientValuesToTrack, 2);
  const double expected_average_ambient =
      (15.0 + 7.0) / Adapter::kNumberAmbientValuesToTrack;
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(),
                   expected_average_ambient);
  EXPECT_DOUBLE_EQ(adapter_->GetBrighteningThresholdForTesting(),
                   expected_average_ambient * 1.1);
  EXPECT_DOUBLE_EQ(adapter_->GetDarkeningThresholdForTesting(),
                   expected_average_ambient * 0.8);
}

TEST_F(AdapterTest, ImmediateBrightnessTransitionThresholds) {
  std::map<std::string, std::string> params = default_params_;
  params["immediate_brightening_lux_threshold_ratio"] = "0.3";
  params["immediate_darkening_lux_threshold_ratio"] = "0.3";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_EQ(*adapter_->GetGlobalCurveForTesting(), *global_curve_);
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());
  EXPECT_EQ(*adapter_->GetPersonalCurveForTesting(), *personal_curve_);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Brightness is changed after the 1st ALS reading comes in.
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);

  // Another ALS value is received in 1 sec, brightness is changed because it
  // exceeds immediate transition threshold.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_als_reader_.ReportAmbientLightUpdate(17);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 2);

  // Another ALS value is received in 2 sec, brightness is changed because it
  // exceeds immediate transition threshold.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  fake_als_reader_.ReportAmbientLightUpdate(1);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 3);
}

TEST_F(AdapterTest, BrightnessNotUpdatedOnStartup) {
  std::map<std::string, std::string> params = default_params_;
  params["update_brightness_on_startup"] = "false";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // 1st ALS reading doesn't trigger a brightness change.
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 0);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(test_observer_.num_changes(), 0);

  // 2nd ALS comes in so that we have |kAmbientLightShortHorizonSeconds| of
  // data, hence brightness is changed.
  fake_als_reader_.ReportAmbientLightUpdate(20);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);
}

TEST_F(AdapterTest, UsePersonalCurve) {
  std::map<std::string, std::string> params = default_params_;
  params["model_curve"] = "1";

  // Init modeller with only a global curve.
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, base::nullopt /* personal_curve */, params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  // ALS comes in but no brightness change is triggered because there is no
  // personal curve.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 0);

  // Personal curve is received.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_modeller_.ReportModelTrained(*personal_curve_);
  EXPECT_EQ(test_observer_.num_changes(), 0);
  fake_als_reader_.ReportAmbientLightUpdate(20);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);
  EXPECT_EQ(test_observer_.GetCause(),
            power_manager::BacklightBrightnessChange_Cause_MODEL);

  const double expected_average_ambient = (10 + 20) / 2.0;
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(),
                   expected_average_ambient);
  const double expected_brightness_percent =
      personal_curve_->Interpolate(ConvertToLog(expected_average_ambient));
  EXPECT_DOUBLE_EQ(test_observer_.GetBrightnessPercent(),
                   expected_brightness_percent);
}

TEST_F(AdapterTest, UseGlobalCurve) {
  std::map<std::string, std::string> params = default_params_;
  params["model_curve"] = "0";

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 1);
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(), 10);

  const double expected_brightness_percent1 =
      global_curve_->Interpolate(ConvertToLog(10));
  EXPECT_DOUBLE_EQ(test_observer_.GetBrightnessPercent(),
                   expected_brightness_percent1);

  // A new personal curve is received but adapter still uses the global curve.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  fake_modeller_.ReportModelTrained(*personal_curve_);
  fake_als_reader_.ReportAmbientLightUpdate(20);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 2);
  EXPECT_EQ(test_observer_.GetCause(),
            power_manager::BacklightBrightnessChange_Cause_MODEL);

  const double expected_average_ambient2 = (10 + 20) / 2.0;
  EXPECT_DOUBLE_EQ(adapter_->GetAverageAmbientForTesting(),
                   expected_average_ambient2);
  const double expected_brightness_percent2 =
      global_curve_->Interpolate(ConvertToLog(expected_average_ambient2));
  EXPECT_DOUBLE_EQ(test_observer_.GetBrightnessPercent(),
                   expected_brightness_percent2);
}

TEST_F(AdapterTest, BrightnessSetByPolicy) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, default_params_,
       true /* brightness_set_by_policy */);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kSuccess);

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 0);
}

TEST_F(AdapterTest, FeatureDisabled) {
  // An empty param map will not enable the experiment.
  std::map<std::string, std::string> empty_params;

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       global_curve_, personal_curve_, empty_params);

  EXPECT_EQ(adapter_->GetStatusForTesting(), Adapter::Status::kDisabled);
  // Global and personal curves are received, but they won't be used to change
  // brightness.
  EXPECT_TRUE(adapter_->GetGlobalCurveForTesting());
  EXPECT_TRUE(adapter_->GetPersonalCurveForTesting());

  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Brightness not changed after the 1st ALS reading comes in.
  fake_als_reader_.ReportAmbientLightUpdate(10);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(test_observer_.num_changes(), 0);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
