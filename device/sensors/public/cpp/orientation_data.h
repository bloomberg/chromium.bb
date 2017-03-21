// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_PUBLIC_CPP_ORIENTATION_DATA_H_
#define DEVICE_SENSORS_PUBLIC_CPP_ORIENTATION_DATA_H_

namespace device {

#pragma pack(push, 1)

class OrientationData {
 public:
  OrientationData();
  ~OrientationData() {}

  double alpha;
  double beta;
  double gamma;

  bool hasAlpha : 1;
  bool hasBeta : 1;
  bool hasGamma : 1;

  bool absolute : 1;

  bool allAvailableSensorsAreActive : 1;
};

static_assert(sizeof(OrientationData) ==
                  (3 * sizeof(double) + 1 * sizeof(char)),
              "OrientationData has wrong size");

#pragma pack(pop)

}  // namespace device

#endif  // DEVICE_SENSORS_PUBLIC_CPP_ORIENTATION_DATA_H_
