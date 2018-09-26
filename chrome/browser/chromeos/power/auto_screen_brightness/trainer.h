// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// It is a mapping from ambient light (in lux) to brightness percent. It should
// be sorted in the ascending order in lux.
using BrightnessCurve = std::vector<std::pair<double, double>>;

struct TrainingDataPoint {
  double brightness_old;
  double brightness_new;
  double ambient_lux;
  base::TimeTicks sample_time;
};

// Interface to train an on-device adaptive brightness curve.
class Trainer {
 public:
  virtual ~Trainer() = default;

  virtual BrightnessCurve Train(const BrightnessCurve& curve,
                                const std::vector<TrainingDataPoint>& data) = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_
