// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MONOTONE_CUBIC_SPLINE_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MONOTONE_CUBIC_SPLINE_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// This class implements monotone cubic spline from Fritsch-Carlson (1980), see
// https://en.wikipedia.org/wiki/Monotone_cubic_interpolation
// This class only supports non-decreasing sequence of control points.
class MonotoneCubicSpline {
 public:
  // |xs| and |ys| must have the same size with at least 2 elements. |xs| must
  // be strictly increasing and |ys| must be monotone (non-decreasing).
  MonotoneCubicSpline(const std::vector<double>& xs,
                      const std::vector<double>& ys);

  MonotoneCubicSpline(const MonotoneCubicSpline& spline);

  ~MonotoneCubicSpline();

  // Returns interpolated value for |x|. If |x| is smaller|greater than
  // smallest|largest value in |xs_|, then smallest|largest value in |ys_| will
  // be returned.
  double Interpolate(double x) const;

  std::vector<double> GetControlPointsX() const;

  std::vector<double> GetControlPointsY() const;

 private:
  const std::vector<double> xs_;
  const std::vector<double> ys_;

  const size_t num_points_;

  // Tangents of control points.
  const std::vector<double> ms_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MONOTONE_CUBIC_SPLINE_H_
