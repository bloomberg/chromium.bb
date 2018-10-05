// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <algorithm>
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

  // Fail the action if selectors is empty.
  if (proto_.wait_for_dom().selectors().empty()) {
    UpdateProcessedAction(OTHER_ACTION_STATUS);
    DLOG(ERROR) << "Empty selector, failing action.";
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  int check_rounds = kDefaultCheckRounds;

  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    check_rounds = std::min(
        1,
        static_cast<int>(std::ceil(timeout_ms / kCheckPeriodInMilliseconds)));

  CheckElementExists(delegate, check_rounds, std::move(callback));
}

void WaitForDomAction::CheckElementExists(ActionDelegate* delegate,
                                          int rounds,
                                          ProcessActionCallback callback) {
  DCHECK_GT(rounds, 0);
  std::vector<std::string> selectors;
  for (const auto& selector : proto_.wait_for_dom().selectors()) {
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
  if (result) {
    UpdateProcessedAction(ACTION_APPLIED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  --rounds;
  if (rounds <= 0) {
    DCHECK_EQ(rounds, 0);
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WaitForDomAction::CheckElementExists,
                     weak_ptr_factory_.GetWeakPtr(), delegate, rounds,
                     std::move(callback)),
      base::TimeDelta::FromMilliseconds(kCheckPeriodInMilliseconds));
}

}  // namespace autofill_assistant
