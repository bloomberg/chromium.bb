// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "url/gurl.h"

namespace autofill_assistant {

PromptAction::PromptAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_prompt());
}

PromptAction::~PromptAction() {}

void PromptAction::InternalProcessAction(ActionDelegate* delegate,
                                         ProcessActionCallback callback) {
  delegate->ShowStatusMessage(proto_.prompt().message());
  DCHECK_GT(proto_.prompt().suggestion_size(), 0);

  callback_ = std::move(callback);
  delegate->Choose(ExtractVector(proto_.prompt().suggestion()),
                   base::BindOnce(&PromptAction::OnSuggestionChosen,
                                  weak_ptr_factory_.GetWeakPtr()));

  if (proto_.prompt().auto_select_size() > 0) {
    batch_element_checker_ = delegate->CreateBatchElementChecker();
    for (const auto& auto_select : proto_.prompt().auto_select()) {
      batch_element_checker_->AddElementCheck(
          kExistenceCheck, ExtractSelector(auto_select.element()),
          base::BindOnce(&PromptAction::OnElementExist, base::Unretained(this),
                         auto_select.result()));
    }
    // Wait as long as necessary for one of the elements to show up. This is
    // cancelled by OnSuggestionChosen()
    batch_element_checker_->Run(
        base::TimeDelta::Max(),
        /* try_done= */
        base::BindRepeating(&PromptAction::OnElementChecksDone,
                            base::Unretained(this), base::Unretained(delegate)),
        /* all_done= */ base::DoNothing());
  }
}

void PromptAction::OnElementExist(const std::string& result, bool exists) {
  if (exists)
    forced_result_ = result;

  // Calling ForceChoose is delayed until try_done, as it indirectly deletes
  // batch_element_checker_, which isn't supported from an element check
  // callback.
}

void PromptAction::OnElementChecksDone(ActionDelegate* delegate) {
  if (!forced_result_.empty())
    delegate->ForceChoose(forced_result_);
}

void PromptAction::OnSuggestionChosen(const std::string& chosen) {
  batch_element_checker_.reset();
  UpdateProcessedAction(ACTION_APPLIED);
  processed_action_proto_->mutable_prompt_result()->set_result(chosen);
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
