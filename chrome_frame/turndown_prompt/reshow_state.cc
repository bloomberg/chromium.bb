// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/turndown_prompt/reshow_state.h"

#include "base/win/registry.h"
#include "chrome_frame/utils.h"

namespace {

const wchar_t kTurndownPromptLastShown[] = L"TurndownPromptLastShown";

}

namespace turndown_prompt {

ReshowState::ReshowState(const base::TimeDelta& reshow_delta)
    : reshow_delta_(reshow_delta) {
}

ReshowState::~ReshowState() {
}

bool ReshowState::HasReshowDeltaExpired(const base::Time& current_time) const {
  int64 last_shown = GetConfigInt64(0LL, kTurndownPromptLastShown);
  if (!last_shown)
    return true;

  base::Time last_shown_time(base::Time::FromInternalValue(last_shown));

  return current_time - last_shown_time >= reshow_delta_;
}

void ReshowState::MarkShown(const base::Time& last_shown_time) {
  SetConfigInt64(kTurndownPromptLastShown, last_shown_time.ToInternalValue());
}

}  // namespace turndown_prompt
