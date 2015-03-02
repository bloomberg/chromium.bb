// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_
#define CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_

#include <jni.h>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

class UserActionRateCounter;

// The native part of java UmaSessionStats class.
class UmaSessionStats {
 public:
  UmaSessionStats();

  void UmaResumeSession(JNIEnv* env, jobject obj);
  void UmaEndSession(JNIEnv* env, jobject obj);

  static void RegisterSyntheticFieldTrialWithNameHash(
      uint32_t trial_name_hash,
      const std::string& group_name);

  static void RegisterSyntheticFieldTrial(
      const std::string& trial_name,
      const std::string& group_name);

 private:
  ~UmaSessionStats();

  // Start of the current session, used for UMA.
  base::TimeTicks session_start_time_;
  int active_session_count_;

  DISALLOW_COPY_AND_ASSIGN(UmaSessionStats);
};

// Registers the native methods through jni
bool RegisterUmaSessionStats(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_
