// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_PIP_UMA_H_
#define ASH_METRICS_PIP_UMA_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

constexpr char kAshPipEventsHistogramName[] = "Ash.Pip.Events";

// This enum should be kept in sync with the AshPipEvents enum in
// src/tools/metrics/histograms/enums.xml.
enum class AshPipEvents {
  PIP_START = 0,
  PIP_END = 1,
  ANDROID_PIP_START = 2,
  ANDROID_PIP_END = 3,
  CHROME_PIP_START = 4,
  CHROME_PIP_END = 5,
  FREE_RESIZE = 6,
  COUNT = 7
};

}  // namespace ash

#endif  // ASH_METRICS_PIP_UMA_H_
