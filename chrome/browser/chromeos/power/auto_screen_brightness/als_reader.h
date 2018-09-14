// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_

#include "base/observer_list_types.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Interface to ambient light reader.
class AlsReader {
 public:
  // Status of AlsReader initialization.
  enum class AlsInitStatus {
    kSuccess = 0,
    kInProgress = 1,
    kDisabled = 2,
    kIncorrectConfig = 3,
    kMissingPath = 4,
    kMaxValue = kMissingPath
  };

  // Observers should take WeakPtr of AlsReader and remove themselves
  // in observers' destructors if AlsReader hasn't be destructed.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;
    virtual void OnAmbientLightUpdated(int lux) = 0;
    virtual void OnAlsReaderInitialized(AlsInitStatus status) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~AlsReader() = default;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // An observer can call this method to check if ALS has been properly
  // initialized and ready to use.
  virtual AlsInitStatus GetInitStatus() const = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_READER_H_
