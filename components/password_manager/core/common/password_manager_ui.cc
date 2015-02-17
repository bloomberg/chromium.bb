// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/password_manager/core/common/password_manager_ui.h"

namespace password_manager {

namespace ui {

bool IsAskSubmitURLState(State state) {
  return state == ASK_USER_REPORT_URL_BUBBLE_SHOWN_BEFORE_TRANSITION_STATE ||
         state == ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE;
}

}  // namespace ui

}  // namespace password_manager
