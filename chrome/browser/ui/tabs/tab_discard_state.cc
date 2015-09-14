// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_discard_state.h"

#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

const char kDiscardStateKey[] = "TabDiscardState";

}  // namespace

// static
TabDiscardState* TabDiscardState::Get(WebContents* web_contents) {
  TabDiscardState* discard_state = static_cast<TabDiscardState*>(
      web_contents->GetUserData(&kDiscardStateKey));

  // If this function is called, we probably need to query/change the discard
  // state. Let's go ahead a add one.
  if (!discard_state) {
    discard_state = new TabDiscardState;
    web_contents->SetUserData(&kDiscardStateKey, discard_state);
  }

  return discard_state;
}

// static
void TabDiscardState::Set(WebContents* web_contents, TabDiscardState* state) {
  web_contents->SetUserData(&kDiscardStateKey, state);
}

// static
bool TabDiscardState::IsDiscarded(WebContents* web_contents) {
  TabDiscardState* discard_state = TabDiscardState::Get(web_contents);
  return discard_state->is_discarded_;
}

// static
void TabDiscardState::SetDiscardState(WebContents* web_contents, bool state) {
  TabDiscardState* discard_state = TabDiscardState::Get(web_contents);
  discard_state->is_discarded_ = state;
}

// static
int TabDiscardState::DiscardCount(WebContents* web_contents) {
  TabDiscardState* discard_state = TabDiscardState::Get(web_contents);
  return discard_state->discard_count_;
}

// static
void TabDiscardState::IncrementDiscardCount(WebContents* web_contents) {
  TabDiscardState* discard_state = TabDiscardState::Get(web_contents);
  discard_state->discard_count_++;
}

// static
void TabDiscardState::CopyState(content::WebContents* old_contents,
                                content::WebContents* new_contents) {
  TabDiscardState* old_state = Get(old_contents);
  TabDiscardState* new_State = Get(new_contents);
  *new_State = *old_state;
}
