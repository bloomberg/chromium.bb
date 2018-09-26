// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"

#include <cmath>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {
// Loads data from a specified location on disk. This should run in another
// thread to be non-blocking to the main thread (if |is_testing| is false).
std::string LoadDataFromDisk(const base::FilePath& path, bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!PathExists(path)) {
    return "";
  }

  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    return "";
  }
  return content;
}

// Saves data to disk. This should run in another thread to be non-blocking to
// the main thread (if |is_testing| is false).
// TODO(jiameng): alternative to WriteFile is WriteFileAtomically, but the
// latter is very slow. Investigate whether we need to change to
// WriteFileAtomically.
void SaveCurveToDisk(const base::FilePath& path,
                     const BrightnessCurve& curve,
                     bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const std::string data = CurveToString(curve);
  const int bytes_written = base::WriteFile(path, data.data(), data.size());
  if (bytes_written != static_cast<int>(data.size())) {
    LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
               << data.size() << " to " << path.value();
  }
}

// Trains a new curve using the current |curve| and training |data|. Returns the
// new curve. This should run in another thread to be non-blocking to the main
// thread (if |is_testing| is false).
BrightnessCurve TrainModel(Trainer* trainer,
                           const BrightnessCurve& curve,
                           const std::vector<TrainingDataPoint>& data,
                           bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return trainer->Train(curve, data);
}

}  // namespace

constexpr base::TimeDelta ModellerImpl::kTrainingDelay;
constexpr int ModellerImpl::kMaxTrainingDataPoints;
constexpr int ModellerImpl::kAmbientLightHorizonSeconds;
constexpr base::TimeDelta ModellerImpl::kAmbientLightHorizon;
constexpr int ModellerImpl::kNumberAmbientValuesToTrack;
constexpr char ModellerImpl::kModelDir[];
constexpr char ModellerImpl::kCurveFileName[];

ModellerImpl::ModellerImpl(Profile* profile,
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
                        base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
                   base::DefaultTickClock::GetInstance()) {}

ModellerImpl::~ModellerImpl() = default;

void ModellerImpl::AddObserver(Modeller::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (model_status_ != Modeller::Status::kInitializing) {
    observer->OnModelInitialized(model_status_, curve_);
  }
}

void ModellerImpl::RemoveObserver(Modeller::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ModellerImpl::OnAmbientLightUpdated(int lux) {
  if (model_status_ == Status::kDisabled)
    return;

  const AmbientLightSample sample = {lux, tick_clock_->NowTicks()};
  ambient_light_values_.SaveToBuffer(sample);
}

void ModellerImpl::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  if (als_init_status_.has_value())
    return;

  als_init_status_ = status;

  HandleStatusUpdate();
}

void ModellerImpl::OnBrightnessMonitorInitialized(bool success) {
  if (brightness_monitor_success_.has_value())
    return;

  brightness_monitor_success_ = success;
  HandleStatusUpdate();
}

void ModellerImpl::OnUserBrightnessChanged(double old_brightness_percent,
                                           double new_brightness_percent) {
  if (model_status_ == Status::kDisabled)
    return;

  // We don't add any training data if there is no ambient light sample.
  if (ambient_light_values_.CurrentIndex() == 0)
    return;

  const double average_ambient_lux = AverageAmbient(ambient_light_values_, -1);
  data_cache_.push_back({old_brightness_percent, new_brightness_percent,
                         average_ambient_lux, tick_clock_->NowTicks()});

  if (data_cache_.size() == kMaxTrainingDataPoints) {
    model_timer_.Stop();
    StartTraining();
  }
}

void ModellerImpl::OnUserBrightnessChangeRequested() {}

void ModellerImpl::OnUserActivity(const ui::Event* event) {
  if (!event)
    return;
  ScheduleTrainerStart();
}

std::unique_ptr<ModellerImpl> ModellerImpl::CreateForTesting(
    Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    ui::UserActivityDetector* user_activity_detector,
    std::unique_ptr<Trainer> trainer,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  return base::WrapUnique(new ModellerImpl(
      profile, als_reader, brightness_monitor, user_activity_detector,
      std::move(trainer), task_runner, tick_clock, true /* is_testing */));
}

double ModellerImpl::AverageAmbientForTesting() const {
  return AverageAmbient(ambient_light_values_, -1);
}

int ModellerImpl::NumberTrainingDataPointsForTesting() const {
  return data_cache_.size();
}

base::FilePath ModellerImpl::GetCurvePathForTesting(Profile* profile) const {
  return GetCurvePathFromProfile(profile);
}

ModellerImpl::ModellerImpl(
    Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    ui::UserActivityDetector* user_activity_detector,
    std::unique_ptr<Trainer> trainer,
    const scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock,
    bool is_testing)
    : is_testing_(is_testing),
      als_reader_observer_(this),
      brightness_monitor_observer_(this),
      user_activity_observer_(this),
      model_task_runner_(task_runner),
      trainer_(trainer.release(),
               base::OnTaskRunnerDeleter(model_task_runner_)),
      tick_clock_(tick_clock),
      model_timer_(tick_clock_),
      weak_ptr_factory_(this) {
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(trainer_);
  DCHECK(user_activity_detector);

  if (!profile) {
    model_status_ = Status::kDisabled;
    return;
  }

  curve_path_ = GetCurvePathFromProfile(profile);
  if (curve_path_.empty()) {
    model_status_ = Status::kDisabled;
    return;
  }

  model_timer_.SetTaskRunner(model_task_runner_);

  als_reader_observer_.Add(als_reader);
  brightness_monitor_observer_.Add(brightness_monitor);
  user_activity_observer_.Add(user_activity_detector);
}

base::FilePath ModellerImpl::GetCurvePathFromProfile(Profile* profile) const {
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

void ModellerImpl::HandleStatusUpdate() {
  if (model_status_ != Modeller::Status::kInitializing)
    return;

  if (!als_init_status_.has_value()) {
    return;
  }
  const bool als_success =
      als_init_status_.value() == AlsReader::AlsInitStatus::kSuccess;
  if (!als_success) {
    model_status_ = Modeller::Status::kDisabled;
    OnInitializationComplete();
    return;
  }

  if (!brightness_monitor_success_.has_value()) {
    return;
  }
  if (!brightness_monitor_success_.value()) {
    model_status_ = Modeller::Status::kDisabled;
    OnInitializationComplete();
    return;
  }

  base::PostTaskAndReplyWithResult(
      model_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadDataFromDisk, curve_path_, is_testing_),
      base::BindOnce(&ModellerImpl::OnCurveLoadedFromDisk,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModellerImpl::OnInitializationComplete() {
  DCHECK_NE(model_status_, Status::kInitializing);
  for (auto& observer : observers_)
    observer.OnModelInitialized(model_status_, curve_);
}

// TODO(crbug.com/881215): add actual default curve and then add unit test too.
void ModellerImpl::InitWithDefaultCurve() {
  curve_.clear();
  return;
}

void ModellerImpl::OnCurveLoadedFromDisk(const std::string& content) {
  if (content.empty() || !CurveFromString(content, &curve_)) {
    InitWithDefaultCurve();
    model_status_ = Status::kGlobal;
  } else {
    model_status_ = Status::kPersonal;
  }

  OnInitializationComplete();

  ScheduleTrainerStart();
}

void ModellerImpl::ScheduleTrainerStart() {
  // Reset the timer if it's already running.
  model_timer_.Start(FROM_HERE, kTrainingDelay, this,
                     &ModellerImpl::StartTraining);
}

void ModellerImpl::StartTraining() {
  if (data_cache_.empty()) {
    ScheduleTrainerStart();
    return;
  }

  base::PostTaskAndReplyWithResult(
      model_task_runner_.get(), FROM_HERE,
      base::BindOnce(&TrainModel, trainer_.get(), curve_,
                     std::move(data_cache_), is_testing_),
      base::BindOnce(&ModellerImpl::OnTrainingFinished,
                     weak_ptr_factory_.GetWeakPtr()));
  data_cache_.clear();
}

void ModellerImpl::OnTrainingFinished(const BrightnessCurve& curve) {
  curve_ = curve;
  for (auto& observer : observers_)
    observer.OnModelTrained(curve);

  model_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveCurveToDisk, curve_path_, curve_, is_testing_));
  ScheduleTrainerStart();
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
