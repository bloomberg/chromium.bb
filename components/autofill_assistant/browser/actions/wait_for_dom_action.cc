// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {
int kCheckPeriodInMilliseconds = 100;

// So it takes about 150*100 milliseconds.
int kDefaultCheckRounds = 150;
}  // namespace

namespace autofill_assistant {

WaitForDomAction::WaitForDomAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {}

WaitForDomAction::~WaitForDomAction() {}

void WaitForDomAction::ProcessAction(ActionDelegate* delegate,
                                     ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  int check_rounds = kDefaultCheckRounds;

  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    check_rounds = std::ceil(timeout_ms / kCheckPeriodInMilliseconds);

  CheckElementExists(delegate, check_rounds, std::move(callback));
}

void WaitForDomAction::CheckElementExists(ActionDelegate* delegate,
                                          int rounds,
                                          ProcessActionCallback callback) {
  DCHECK(rounds > 0);
  std::vector<std::string> selectors;
  for (const auto& selector : proto_.wait_for_dom().element().selectors()) {
    selectors.emplace_back(selector);
  }
  delegate->ElementExists(
      selectors, base::BindOnce(&WaitForDomAction::OnCheckElementExists,
                                weak_ptr_factory_.GetWeakPtr(), delegate,
                                rounds, std::move(callback)));
}

void WaitForDomAction::OnCheckElementExists(ActionDelegate* delegate,
                                            int rounds,
                                            ProcessActionCallback callback,
                                            bool result) {
  bool for_absence = proto_.wait_for_dom().check_for_absence();
  if (for_absence && !result) {
    UpdateProcessedAction(true);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  if (!for_absence && result) {
    UpdateProcessedAction(true);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  if (rounds == 0) {
    UpdateProcessedAction(false);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  --rounds;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WaitForDomAction::CheckElementExists,
                     weak_ptr_factory_.GetWeakPtr(), delegate, rounds,
                     std::move(callback)),
      base::TimeDelta::FromMilliseconds(kCheckPeriodInMilliseconds));
}

}  // namespace autofill_assistant.
