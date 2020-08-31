// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_SESSION_COMPLETION_LOGGER_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_SESSION_COMPLETION_LOGGER_H_

#include "base/macros.h"

namespace chromeos {

namespace tether {

// Wrapper around metrics reporting for how a Tether session ended.
class TetherSessionCompletionLogger {
 public:
  enum SessionCompletionReason {
    OTHER = 0,
    USER_DISCONNECTED = 1,
    CONNECTION_DROPPED = 2,
    USER_LOGGED_OUT = 3,
    USER_CLOSED_LID = 4,
    PREF_DISABLED = 5,
    BLUETOOTH_DISABLED = 6,
    CELLULAR_DISABLED = 7,
    WIFI_DISABLED = 8,
    BLUETOOTH_CONTROLLER_DISAPPEARED = 9,
    MULTIDEVICE_HOST_UNVERIFIED = 10,
    BETTER_TOGETHER_SUITE_DISABLED = 11,
    SESSION_COMPLETION_REASON_MAX
  };

  TetherSessionCompletionLogger();
  virtual ~TetherSessionCompletionLogger();

  virtual void RecordTetherSessionCompletion(
      const SessionCompletionReason& reason);

 private:
  friend class TetherSessionCompletionLoggerTest;

  DISALLOW_COPY_AND_ASSIGN(TetherSessionCompletionLogger);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_SESSION_COMPLETION_LOGGER_H_
