// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_UTILS_H_

#include <string>
#include <vector>

#include "base/containers/ring_buffer.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/trainer.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Replaces |curve| with the parsed curve data from |data|, returning true if
// successful. Correct formatting in |data| should be 1 row per
// (<ambient_light>, <brightness>) mapping. Ambient light should be a double and
// brightness should be a double.
bool CurveFromString(const std::string& data, BrightnessCurve* const curve);

// Converts |curve| to a string. An empty string will be returned if |curve| is
// empty.
std::string CurveToString(const BrightnessCurve& curve);

struct AmbientLightSample {
  int lux;
  base::TimeTicks sample_time;
};

// Calculates average ambient light over the most recent |num_recent| samples.
// |num_recent| has to be no larger than the capacity of the buffer, but it can
// be greater than the number of samples stored. If |num_recent| is greater than
// the number of samples stored, it returns average of stored samples. If
// |num_recent| is negative, it returns average over all stored samples. If
// input |ambient_light_values| is empty, it returns 0.
// TODO(jiameng): this is the very basic version of averaging, we need to change
// it to allow for time decay.
template <size_t size>
double AverageAmbient(
    const base::RingBuffer<AmbientLightSample, size>& ambient_light_values,
    int num_recent) {
  DCHECK_LE(num_recent, static_cast<int>(ambient_light_values.BufferSize()));
  if (ambient_light_values.CurrentIndex() == 0)
    return 0;

  double sum = 0;
  int count = 0;
  const int used_num_recent =
      num_recent < 0 ? ambient_light_values.BufferSize() : num_recent;
  for (auto it = ambient_light_values.End(); it && count < used_num_recent;
       --it) {
    const auto& ambient_sample = **it;
    sum += ambient_sample.lux;
    ++count;
  }
  return sum / count;
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_UTILS_H_
