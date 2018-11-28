// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"

#include <cmath>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chromeos/chromeos_features.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// Creates a global/default brightness curve.
// TODO(crbug.com/881215): default curve may be revised, if so, need to update
// unit tests as well.
MonotoneCubicSpline CreateGlobalCurve() {
  const std::string global_curve = GetFieldTrialParamValueByFeature(
      features::kAutoScreenBrightness, "global_curve");
  if (!global_curve.empty()) {
    const base::Optional<MonotoneCubicSpline> global_curve_spline =
        MonotoneCubicSpline::FromString(global_curve);
    if (global_curve_spline)
      return *global_curve_spline;
    // TODO(jiameng): log error to UMA.
  }

  const std::vector<double> default_log_lux = {
      3.69, 4.83, 6.54, 7.68, 8.25, 8.82,
  };

  const std::vector<double> default_brightness = {
      36.14, 47.62, 85.83, 93.27, 93.27, 100,
  };

  return MonotoneCubicSpline(default_log_lux, default_brightness);
}

// Loads curve from a specified location on disk. This should run in another
// thread to be non-blocking to the main thread (if |is_testing| is false).
// The ambient values read from disk should be in the log-domain already.
base::Optional<MonotoneCubicSpline> LoadCurveFromDisk(
    const base::FilePath& path,
    bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!PathExists(path)) {
    return base::nullopt;
  }

  std::string content;
  if (!base::ReadFileToString(path, &content) || content.empty()) {
    return base::nullopt;
  }

  return MonotoneCubicSpline::FromString(content);
}

// Saves |curve| to disk and returns whether it was successful. This should run
// in another thread to be non-blocking to the main thread (if |is_testing| is
// false).
// TODO(jiameng): write to a temp location and rename to |path|. Refactor out
// the logic that's shared with the unit test to utils.
bool SaveCurveToDisk(const base::FilePath& path,
                     const MonotoneCubicSpline& curve,
                     bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const std::string data = curve.ToString();
  DCHECK(!data.empty());
  const int bytes_written = base::WriteFile(path, data.data(), data.size());
  if (bytes_written != static_cast<int>(data.size())) {
    LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
               << data.size() << " to " << path.value();
    return false;
  }
  return true;
}

// Trains a new curve using training |data| and returns the new curve. This
// should only be called after trainer has been initialized with a global curve
// and a latest curve.
// This should run in another thread to be non-blocking to the main
// thread (if |is_testing| is false).
MonotoneCubicSpline TrainModel(Trainer* trainer,
                               const std::vector<TrainingDataPoint>& data,
                               bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return trainer->Train(data);
}

// Sets initial global and personal curve.
// This should run in another thread to be non-blocking to the main
// thread (if |is_testing| is false).
bool SetInitialCurves(Trainer* trainer,
                      const MonotoneCubicSpline& global_curve,
                      const MonotoneCubicSpline& current_curve,
                      bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return trainer->SetInitialCurves(global_curve, current_curve);
}

}  // namespace

constexpr int ModellerImpl::kAmbientLightHorizonSeconds;
constexpr base::TimeDelta ModellerImpl::kAmbientLightHorizon;
constexpr int ModellerImpl::kNumberAmbientValuesToTrack;
constexpr char ModellerImpl::kModelDir[];
constexpr char ModellerImpl::kCurveFileName[];

ModellerImpl::ModellerImpl(const Profile* profile,
                           AlsReader* als_reader,
                           BrightnessMonitor* brightness_monitor,
                           ui::UserActivityDetector* user_activity_detector,
                           std::unique_ptr<Trainer> trainer)
    : ModellerImpl(profile,
                   als_reader,
                   brightness_monitor,
                   user_activity_detector,
                   std::move(trainer),
                   base::CreateSequencedTaskRunnerWithTraits(
                       {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                        base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
                   base::DefaultTickClock::GetInstance()) {}

ModellerImpl::~ModellerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ModellerImpl::AddObserver(Modeller::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (is_modeller_enabled_.has_value()) {
    NotifyObserverInitStatus(*observer);
  }
}

void ModellerImpl::RemoveObserver(Modeller::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ModellerImpl::OnAmbientLightUpdated(int lux) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_modeller_enabled_.has_value() && !*is_modeller_enabled_)
    return;

  const AmbientLightSample sample = {lux, tick_clock_->NowTicks()};
  ambient_light_values_.SaveToBuffer(sample);
}

void ModellerImpl::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!als_init_status_);

  als_init_status_ = status;

  HandleStatusUpdate();
}

void ModellerImpl::OnBrightnessMonitorInitialized(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!brightness_monitor_success_.has_value());

  brightness_monitor_success_ = success;
  HandleStatusUpdate();
}

void ModellerImpl::OnUserBrightnessChanged(double old_brightness_percent,
                                           double new_brightness_percent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_modeller_enabled_.has_value() && !*is_modeller_enabled_)
    return;

  // We don't add any training data if there is no ambient light sample.
  if (ambient_light_values_.CurrentIndex() == 0)
    return;

  const double average_ambient_lux = AverageAmbient(ambient_light_values_, -1);
  data_cache_.push_back({old_brightness_percent, new_brightness_percent,
                         ConvertToLog(average_ambient_lux),
                         tick_clock_->NowTicks()});

  ScheduleTrainerStart();
}

void ModellerImpl::OnUserBrightnessChangeRequested() {}

void ModellerImpl::OnUserActivity(const ui::Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event)
    return;
  ScheduleTrainerStart();
}

std::unique_ptr<ModellerImpl> ModellerImpl::CreateForTesting(
    const Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    ui::UserActivityDetector* user_activity_detector,
    std::unique_ptr<Trainer> trainer,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::TickClock* tick_clock) {
  return base::WrapUnique(new ModellerImpl(
      profile, als_reader, brightness_monitor, user_activity_detector,
      std::move(trainer), blocking_task_runner, tick_clock,
      true /* is_testing */));
}

double ModellerImpl::AverageAmbientForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AverageAmbient(ambient_light_values_, -1);
}

size_t ModellerImpl::NumberTrainingDataPointsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_cache_.size();
}

MonotoneCubicSpline ModellerImpl::GetGlobalCurveForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return global_curve_;
}

size_t ModellerImpl::GetMaxTrainingDataPointsForTesting() const {
  return max_training_data_points_;
}

base::TimeDelta ModellerImpl::GetTrainingDelayForTesting() const {
  return training_delay_;
}

base::FilePath ModellerImpl::GetCurvePathFromProfile(const Profile* profile) {
  DCHECK(profile);
  const base::FilePath empty_path;

  const base::FilePath profile_path = profile->GetPath();
  if (profile_path.empty()) {
    return empty_path;
  }

  const base::FilePath model_dir = profile_path.Append(kModelDir);
  if (!base::DirectoryExists(model_dir) && !base::CreateDirectory(model_dir)) {
    return empty_path;
  }

  return model_dir.Append(kCurveFileName);
}

ModellerImpl::ModellerImpl(
    const Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    ui::UserActivityDetector* user_activity_detector,
    std::unique_ptr<Trainer> trainer,
    const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::TickClock* tick_clock,
    bool is_testing)
    : is_testing_(is_testing),
      als_reader_observer_(this),
      brightness_monitor_observer_(this),
      user_activity_observer_(this),
      blocking_task_runner_(blocking_task_runner),
      trainer_(trainer.release(),
               base::OnTaskRunnerDeleter(blocking_task_runner_)),
      tick_clock_(tick_clock),
      model_timer_(tick_clock_),
      global_curve_(CreateGlobalCurve()),
      weak_ptr_factory_(this) {
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(trainer_);
  DCHECK(user_activity_detector);

  if (!profile) {
    is_modeller_enabled_ = false;
    return;
  }

  if (!trainer_->HasValidConfiguration()) {
    is_modeller_enabled_ = false;
    return;
  }

  curve_path_ = GetCurvePathFromProfile(profile);
  if (curve_path_.empty()) {
    is_modeller_enabled_ = false;
    return;
  }

  als_reader_observer_.Add(als_reader);
  brightness_monitor_observer_.Add(brightness_monitor);
  user_activity_observer_.Add(user_activity_detector);

  const int max_training_data_points = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "max_training_data_points", -1);
  if (max_training_data_points > 0) {
    max_training_data_points_ = max_training_data_points;
  }

  const int training_delay_in_seconds = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "training_delay_in_seconds",
      training_delay_.InSeconds());
  if (training_delay_in_seconds >= 0) {
    training_delay_ = base::TimeDelta::FromSeconds(training_delay_in_seconds);
  }
}


void ModellerImpl::HandleStatusUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_modeller_enabled_.has_value())
    return;

  if (!als_init_status_)
    return;

  const bool als_success =
      *als_init_status_ == AlsReader::AlsInitStatus::kSuccess;
  if (!als_success) {
    is_modeller_enabled_ = false;
    OnInitializationComplete();
    return;
  }

  if (!brightness_monitor_success_.has_value()) {
    return;
  }
  if (!*brightness_monitor_success_) {
    is_modeller_enabled_ = false;
    OnInitializationComplete();
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadCurveFromDisk, curve_path_, is_testing_),
      base::BindOnce(&ModellerImpl::OnCurveLoadedFromDisk,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModellerImpl::OnInitializationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(jiameng): log model status to UMA.
  for (auto& observer : observers_) {
    NotifyObserverInitStatus(observer);
  }
}

void ModellerImpl::NotifyObserverInitStatus(Modeller::Observer& observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_modeller_enabled_.has_value());
  if (!*is_modeller_enabled_) {
    observer.OnModelInitialized(base::nullopt, base::nullopt);
  } else {
    base::Optional<MonotoneCubicSpline> personal_curve;
    if (has_initial_personal_curve_)
      personal_curve.emplace(trainer_->GetCurrentCurve());

    observer.OnModelInitialized(global_curve_, personal_curve);
  }
}

void ModellerImpl::OnCurveLoadedFromDisk(
    const base::Optional<MonotoneCubicSpline>& curve) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Run SetInitialCurves calculations on background thread to avoid blocking UI
  // thread.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&SetInitialCurves, trainer_.get(), global_curve_,
                     curve ? *curve : global_curve_, is_testing_),
      base::BindOnce(&ModellerImpl::OnSetInitialCurves,
                     weak_ptr_factory_.GetWeakPtr(), curve));
}

void ModellerImpl::OnCurveSavedToDisk(bool is_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(jiameng): log to UMA.
  DCHECK(is_successful);
}

void ModellerImpl::OnSetInitialCurves(
    const base::Optional<MonotoneCubicSpline>& loaded_curve,
    bool is_personal_curve_valid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_BOOLEAN("AutoScreenBrightness.PersonalCurveValid",
                        is_personal_curve_valid);

  has_initial_personal_curve_ = is_personal_curve_valid && loaded_curve;
  DCHECK(trainer_->GetGlobalCurve() == global_curve_);
  DCHECK(trainer_->GetCurrentCurve() ==
         (has_initial_personal_curve_ ? *loaded_curve : global_curve_));

  is_modeller_enabled_ = true;
  OnInitializationComplete();

  // We may have received a brightness change as a training example before the
  // model is set up. Call |ScheduleTrainerStart| to prepare training.
  ScheduleTrainerStart();
}

void ModellerImpl::ScheduleTrainerStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_modeller_enabled_.has_value() || !*is_modeller_enabled_)
    return;

  if (data_cache_.size() >= max_training_data_points_ ||
      training_delay_.is_zero()) {
    model_timer_.Stop();
    StartTraining();
    return;
  }

  // Reset the timer if it's already running.
  model_timer_.Start(FROM_HERE, training_delay_, this,
                     &ModellerImpl::StartTraining);
}

void ModellerImpl::StartTraining() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data_cache_.empty()) {
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&TrainModel, trainer_.get(), std::move(data_cache_),
                     is_testing_),
      base::BindOnce(&ModellerImpl::OnTrainingFinished,
                     weak_ptr_factory_.GetWeakPtr()));
  data_cache_ = std::vector<TrainingDataPoint>();
}

void ModellerImpl::OnTrainingFinished(const MonotoneCubicSpline& curve) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnModelTrained(curve);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&SaveCurveToDisk, curve_path_, curve, is_testing_),
      base::BindOnce(&ModellerImpl::OnCurveSavedToDisk,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
