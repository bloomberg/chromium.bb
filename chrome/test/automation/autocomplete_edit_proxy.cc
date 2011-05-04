// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/autocomplete_edit_proxy.h"

#include <vector>

#include "base/threading/platform_thread.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

using base::TimeDelta;
using base::TimeTicks;

bool AutocompleteEditProxy::GetText(string16* text) const {
  if (!is_valid())
    return false;
  if (!text) {
    NOTREACHED();
    return false;
  }
  bool result = false;
  sender_->Send(new AutomationMsg_AutocompleteEditGetText(
      handle_, &result, text));
  return result;
}

bool AutocompleteEditProxy::WaitForFocus() const {
  if (!is_valid())
    return false;
  bool edit_exists = false;
  sender_->Send(new AutomationMsg_WaitForAutocompleteEditFocus(
      handle_, &edit_exists));
  return edit_exists;
}

bool AutocompleteEditProxy::SetText(const string16& text) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_AutocompleteEditSetText(
      handle_, text, &result));
  return result;
}

bool AutocompleteEditProxy::IsQueryInProgress(bool* query_in_progress) const {
  if (!is_valid())
    return false;
  if (!query_in_progress) {
    NOTREACHED();
    return false;
  }
  bool edit_exists = false;
  sender_->Send(new AutomationMsg_AutocompleteEditIsQueryInProgress(
      handle_, &edit_exists, query_in_progress));
  return edit_exists;
}

bool AutocompleteEditProxy::WaitForQuery(int wait_timeout_ms) const {
  // TODO(jknotten): use a delayed message / observer instead.
  // See, for example, AutocompleteEditProxy::WaitForFocus.
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout_ms);
  bool query_in_progress;
  while (TimeTicks::Now() - start < timeout) {
    if (IsQueryInProgress(&query_in_progress) && !query_in_progress)
      return true;
    base::PlatformThread::Sleep(automation::kSleepTime);
  }
  // If we get here the query is still in progress.
  return false;
}

bool AutocompleteEditProxy::GetAutocompleteMatches(Matches* matches) const {
  if (!is_valid())
    return false;
  if (!matches) {
    NOTREACHED();
    return false;
  }
  bool edit_exists = false;
  sender_->Send(new AutomationMsg_AutocompleteEditGetMatches(
      handle_, &edit_exists, matches));
  return edit_exists;
}
