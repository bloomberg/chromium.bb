// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_SESSION_LENGTH_LIMITER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_SESSION_LENGTH_LIMITER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/timer.h"

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

// Enforces a session length limit by terminating the session when the limit is
// reached.
class SessionLengthLimiter {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    virtual const base::Time GetCurrentTime() const = 0;
    virtual void StopSession() = 0;
  };

  // Registers preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  SessionLengthLimiter(Delegate* delegate, bool browser_restarted);
  ~SessionLengthLimiter();

 private:
  void OnSessionLengthLimitChanged();

  base::ThreadChecker thread_checker_;

  scoped_ptr<Delegate> delegate_;
  PrefChangeRegistrar pref_change_registrar_;

  scoped_ptr<base::OneShotTimer<SessionLengthLimiter::Delegate> > timer_;
  base::Time session_start_time_;

  DISALLOW_COPY_AND_ASSIGN(SessionLengthLimiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_SESSION_LENGTH_LIMITER_H_
