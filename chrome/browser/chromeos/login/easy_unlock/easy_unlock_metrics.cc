// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace chromeos {

void RecordEasyUnlockLoginEvent(EasyUnlockLoginEvent event) {
  DCHECK_LT(event, EASY_SIGN_IN_LOGIN_EVENT_COUNT);

  UMA_HISTOGRAM_ENUMERATION("EasyUnlock.SignIn.LoginEvent",
                            event,
                            EASY_SIGN_IN_LOGIN_EVENT_COUNT);
}

}  // namespace chromeos
