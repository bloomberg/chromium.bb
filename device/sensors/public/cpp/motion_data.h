// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_PUBLIC_CPP_MOTION_DATA_H_
#define DEVICE_SENSORS_PUBLIC_CPP_MOTION_DATA_H_

namespace device {

#pragma pack(push, 1)

class MotionData {
 public:
  MotionData();
  MotionData(const MotionData& other);
  ~MotionData() {}

  double accelerationX;
  double accelerationY;
  double accelerationZ;

  double accelerationIncludingGravityX;
  double accelerationIncludingGravityY;
  double accelerationIncludingGravityZ;

  double rotationRateAlpha;
  double rotationRateBeta;
  double rotationRateGamma;

  double interval;

  bool hasAccelerationX : 1;
  bool hasAccelerationY : 1;
  bool hasAccelerationZ : 1;

  bool hasAccelerationIncludingGravityX : 1;
  bool hasAccelerationIncludingGravityY : 1;
  bool hasAccelerationIncludingGravityZ : 1;

  bool hasRotationRateAlpha : 1;
  bool hasRotationRateBeta : 1;
  bool hasRotationRateGamma : 1;

  bool allAvailableSensorsAreActive : 1;
};

static_assert(sizeof(MotionData) == (10 * sizeof(double) + 2 * sizeof(char)),
              "MotionData has wrong size");

#pragma pack(pop)

}  // namespace device

#endif  // DEVICE_SENSORS_PUBLIC_CPP_MOTION_DATA_H_
