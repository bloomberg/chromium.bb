// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_wait_for_dom_action.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"
#include "content/public/browser/browser_thread.h"

namespace {
int kCheckPeriodInMilliseconds = 100;

// So it takes about 150*100 milliseconds.
int kDefaultCheckRounds = 150;
}  // namespace

namespace autofill_assistant {

AssistantWaitForDomAction::AssistantWaitForDomAction(
    int timeout_ms,
    const std::vector<std::string>& selectors,
    bool for_absence)
    : timeout_ms_(timeout_ms),
      target_element_selectors_(selectors),
      for_absence_(for_absence),
      weak_ptr_factory_(this) {}

AssistantWaitForDomAction::~AssistantWaitForDomAction() {}

void AssistantWaitForDomAction::ProcessAction(AssistantActionDelegate* delegate,
                                              ProcessActionCallback callback) {
  int check_rounds = kDefaultCheckRounds;
  if (timeout_ms_ > 0)
    check_rounds = std::ceil(timeout_ms_ / kCheckPeriodInMilliseconds);

  CheckElementExists(delegate, check_rounds, std::move(callback));
}

void AssistantWaitForDomAction::CheckElementExists(
    AssistantActionDelegate* delegate,
    int rounds,
    ProcessActionCallback callback) {
  DCHECK(rounds > 0);

  delegate->ElementExists(
      target_element_selectors_,
      base::BindOnce(&AssistantWaitForDomAction::OnCheckElementExists,
                     weak_ptr_factory_.GetWeakPtr(), delegate, rounds,
                     std::move(callback)));
}

void AssistantWaitForDomAction::OnCheckElementExists(
    AssistantActionDelegate* delegate,
    int rounds,
    ProcessActionCallback callback,
    bool result) {
  if (for_absence_ && !result) {
    std::move(callback).Run(true);
    return;
  }

  if (!for_absence_ && result) {
    std::move(callback).Run(true);
    return;
  }

  if (rounds == 0) {
    std::move(callback).Run(false);
    return;
  }

  --rounds;
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&AssistantWaitForDomAction::CheckElementExists,
                     weak_ptr_factory_.GetWeakPtr(), delegate, rounds,
                     std::move(callback)),
      base::TimeDelta::FromMilliseconds(kCheckPeriodInMilliseconds));
}

}  // namespace autofill_assistant.
