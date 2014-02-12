// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IDLE_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_IDLE_DETECTOR_H_

#include "ash/wm/user_activity_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer/timer.h"

namespace chromeos {

class IdleDetector : public ash::UserActivityObserver {
 public:
  IdleDetector(const base::Closure& on_active_callback,
               const base::Closure& on_idle_callback);
  virtual ~IdleDetector();

  void Start(const base::TimeDelta& timeout);

 private:
  // UserActivityObserver overrides:
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

  // Resets |timer_| to fire when we reach our idle timeout.
  void ResetTimer();

  base::OneShotTimer<IdleDetector> timer_;

  base::Closure active_callback_;
  base::Closure idle_callback_;

  base::TimeDelta timeout_;

  DISALLOW_COPY_AND_ASSIGN(IdleDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_IDLE_DETECTOR_H_
