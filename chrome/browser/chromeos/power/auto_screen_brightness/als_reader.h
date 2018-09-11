// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/timer/timer.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// AlsReader periodically reads lux values from the ambient light sensor (ALS)
// if powerd has been configured to use it.
class AlsReader {
 public:
  // ALS file location may not be ready immediately, so we retry every
  // |kAlsFileCheckingInterval| until |kMaxInitialAttempts| is reached, then
  // we give up.
  static constexpr base::TimeDelta kAlsFileCheckingInterval =
      base::TimeDelta::FromSeconds(1);
  static constexpr int kMaxInitialAttempts = 20;

  // Interval for polling ambient light values.
  static constexpr base::TimeDelta kAlsPollInterval =
      base::TimeDelta::FromSeconds(1);

  // Status of AlsReader initialization.
  enum class AlsInitStatus {
    kSuccess = 0,
    kInProgress = 1,
    kDisabled = 2,
    kIncorrectConfig = 3,
    kMissingPath = 4,
    kMaxValue = kMissingPath
  };

  // Observers should take WeakPtr of AlsReader and remove themselves in
  // observers' destructors if AlsReader hasn't be destructed.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;
    virtual void OnAmbientLightUpdated(int lux) = 0;
    virtual void OnAlsReaderInitialized(AlsInitStatus status) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  AlsReader();
  ~AlsReader();

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // An observer can call this method to check if ALS has been properly
  // initialized and ready to use.
  AlsInitStatus GetInitStatus() const;

  // Checks if an ALS is enabled, and if the config is valid . Also
  // reads ambient light file path.
  void Init();

  // Sets the task runner for testing purpose.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Sets ambient light path for testing purpose and initialize. This will cause
  // all the checks to be skipped, i.e. whether ALS is enabled and if config is
  // valid.
  void InitForTesting(const base::FilePath& ambient_light_path);

  base::WeakPtr<AlsReader> AsWeakPtr();

 private:
  friend class AlsReaderTest;

  // Called when we've checked whether ALS is enabled.
  void OnAlsEnableCheckDone(bool is_enabled);

  // Called when we've checked whether ALS config is valid.
  void OnAlsConfigCheckDone(bool is_config_valid);

  // Called when we've tried to read ALS path. If |path| is empty, it would
  // reschedule another attempt up to |kMaxInitialAttempts|.
  void OnAlsPathReadAttempted(const std::string& path);

  // Tries to read ALS path.
  void RetryAlsPath();

  // Notifies all observers with |status_| after AlsReader is initialized.
  void OnInitializationComplete();

  // Polls ambient light periodically and notifies all observers if a sample is
  // read.
  void ReadAlsPeriodically();

  // This is called after ambient light (represented as |data|) is sampled. It
  // parses |data| to int, notifies its observers and starts |als_timer_| for
  // next sample.
  void OnAlsRead(const std::string& data);

  AlsInitStatus status_ = AlsInitStatus::kInProgress;
  base::FilePath ambient_light_path_;
  int num_failed_initialization_ = 0;

  // Timer used to retry initialization and also for periodic ambient light
  // sampling.
  base::OneShotTimer als_timer_;
  scoped_refptr<base::SequencedTaskRunner> als_task_runner_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<AlsReader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AlsReader);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_
