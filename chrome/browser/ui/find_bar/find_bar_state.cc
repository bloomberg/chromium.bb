// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"

// static
string16 FindBarState::GetLastPrepopulateText(Profile* p) {
  FindBarState* state = p->GetFindBarState();
  string16 text = state->last_prepopulate_text();

  if (text.empty() && p->IsOffTheRecord()) {
    // Fall back to the original profile.
    state = p->GetOriginalProfile()->GetFindBarState();
    text = state->last_prepopulate_text();
  }

  return text;
}
