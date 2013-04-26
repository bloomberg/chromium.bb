// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_MAGNIFIER_CONSTANTS_H_
#define ASH_MAGNIFIER_MAGNIFIER_CONSTANTS_H_

namespace ash {

// Note: Do not change these values; UMA and prefs depend on them.
enum MagnifierType {
  MAGNIFIER_FULL    = 1,
  MAGNIFIER_PARTIAL = 2,
};

const int kMaxMagnifierType = 2;

const MagnifierType kDefaultMagnifierType = MAGNIFIER_FULL;

// Factor of magnification scale. For example, when this value is 1.189, scale
// value will be changed x1.000, x1.189, x1.414, x1.681, x2.000, ...
// Note: this value is 2.0 ^ (1 / 4).
const float kMagnificationScaleFactor = 1.18920712f;

}  // namespace ash

#endif  // ASH_MAGNIFIER_MAGNIFIER_CONSTANTS_H_
