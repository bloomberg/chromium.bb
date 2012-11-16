// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/idle.h"

// This currently just reports that we're active on Android. Further
// revisions could check device state to see if we're active but on memory
// constrained devices such as Android we'll often get completely killed if
// Chrome isn't active anyway.
// TODO(yfriedman): Tracking in bug: 114481
void CalculateIdleTime(IdleTimeCallback notify) {
  NOTIMPLEMENTED();
  notify.Run(0);
}

bool CheckIdleStateIsLocked() {
  NOTIMPLEMENTED();
  return false;
}
