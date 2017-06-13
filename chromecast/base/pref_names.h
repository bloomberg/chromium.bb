// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_PREF_NAMES_H_
#define CHROMECAST_BASE_PREF_NAMES_H_

namespace chromecast {
namespace prefs {

extern const char kActiveDCSExperiments[];
extern const char kLatestDCSFeatures[];
extern const char kMetricsIsNewClientID[];
extern const char kOptInStats[];
extern const char kStabilityChildProcessCrashCount[];
extern const char kStabilityKernelCrashCount[];
extern const char kStabilityOtherUserCrashCount[];
extern const char kStabilityRendererCrashCount[];
extern const char kStabilityRendererFailedLaunchCount[];
extern const char kStabilityRendererHangCount[];
extern const char kStabilitySystemUncleanShutdownCount[];

}  // namespace prefs
}  // namespace chromecast

#endif  // CHROMECAST_BASE_PREF_NAMES_H_
