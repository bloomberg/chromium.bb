// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_TIMER_H_
#define COMPONENTS_COMPONENT_UPDATER_TIMER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace component_updater {

class Timer {
 public:
  Timer();
  ~Timer();

  void Start(base::TimeDelta initial_delay,
             base::TimeDelta delay,
             const base::Closure& user_task);

  void Stop();

 private:
  void OnDelay();

  base::ThreadChecker thread_checker_;

  base::OneShotTimer timer_;

  base::TimeDelta delay_;
  base::Closure user_task_;

  DISALLOW_COPY_AND_ASSIGN(Timer);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_TIMER_H_
