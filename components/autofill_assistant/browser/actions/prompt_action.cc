// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/element_precondition.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {
// Time between two chip precondition checks.
static constexpr base::TimeDelta kPreconditionChipCheckInterval =
    base::TimeDelta::FromSeconds(1);
}  // namespace

PromptAction::PromptAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_prompt());
}

PromptAction::~PromptAction() {}

void PromptAction::InternalProcessAction(ActionDelegate* delegate,
                                         ProcessActionCallback callback) {
  if (proto_.prompt().choices_size() == 0) {
    UpdateProcessedAction(OTHER_ACTION_STATUS);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_ = delegate;
  callback_ = std::move(callback);
  delegate->SetStatusMessage(proto_.prompt().message());

  SetupAutoSelect();
  SetupPreconditions();
  CheckPreconditions();
  UpdateChips();
}

void PromptAction::SetupPreconditions() {
  int choice_count = proto_.prompt().choices_size();
  preconditions_.resize(choice_count);
  precondition_results_.resize(choice_count);
  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    preconditions_[i] = std::make_unique<ElementPrecondition>(
        choice_proto.show_only_if_element_exists(),
        choice_proto.show_only_if_form_value_matches());
    precondition_results_[i] = false;
  }
}

void PromptAction::CheckPreconditions() {
  // This method might be called by PostDelayedTask, possibly after
  // OnSuggestionChosen ran. The condition on callback_ makes sure that the
  // caller is still waiting for a response.
  if (!callback_)
    return;

  precondition_checker_ = delegate_->CreateBatchElementChecker();
  for (size_t i = 0; i < preconditions_.size(); i++) {
    preconditions_[i]->Check(precondition_checker_.get(),
                             base::BindOnce(&PromptAction::OnPreconditionResult,
                                            weak_ptr_factory_.GetWeakPtr(), i));
  }
  precondition_checker_->Run(
      // Try once; retries are handled by OnPreconditionChecksDone.
      base::TimeDelta::FromSeconds(0),
      /* try_done= */ base::DoNothing(),
      base::BindOnce(&PromptAction::OnPreconditionChecksDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PromptAction::OnPreconditionResult(size_t choice_index, bool result) {
  if (precondition_results_[choice_index] == result)
    return;

  precondition_results_[choice_index] = result;
  precondition_changed_ = true;
}

void PromptAction::OnPreconditionChecksDone() {
  if (precondition_changed_)
    UpdateChips();

  if (HasNonemptyPreconditions()) {
    // If there are element preconditions, the result can change with time, so
    // schedule another check later on.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PromptAction::CheckPreconditions,
                       weak_ptr_factory_.GetWeakPtr()),
        kPreconditionChipCheckInterval);
  }
}

bool PromptAction::HasNonemptyPreconditions() {
  for (const auto& precondition : preconditions_) {
    if (!precondition->empty())
      return true;
  }
  return false;
}

void PromptAction::UpdateChips() {
  DCHECK(callback_);  // Make sure we're still waiting for a response

  auto chips = std::make_unique<std::vector<Chip>>();
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    if (!precondition_results_[i])  // chip disabled
      continue;

    auto& choice_proto = proto_.prompt().choices(i);
    chips->emplace_back();
    Chip& chip = chips->back();
    chip.text = choice_proto.name();
    chip.type = choice_proto.chip_type();
    chips->back().callback = base::BindOnce(&PromptAction::OnSuggestionChosen,
                                            weak_ptr_factory_.GetWeakPtr(), i);
  }
  SetDefaultChipType(chips.get());
  delegate_->Prompt(std::move(chips));
  precondition_changed_ = false;
}

void PromptAction::SetupAutoSelect() {
  // Wait as long as necessary for one of the elements to show up. This is
  // cancelled by CancelProto()
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    Selector selector(
        proto_.prompt().choices(i).auto_select_if_element_exists());
    if (selector.empty())
      continue;

    if (!auto_select_checker_)
      auto_select_checker_ = delegate_->CreateBatchElementChecker();

    auto_select_checker_->AddElementCheck(
        kExistenceCheck, selector,
        base::BindOnce(&PromptAction::OnAutoSelectElementExists,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  if (!auto_select_checker_)
    return;

  auto_select_checker_->Run(base::TimeDelta::Max(),
                            /* try_done= */
                            base::BindRepeating(&PromptAction::OnAutoSelectDone,
                                                weak_ptr_factory_.GetWeakPtr()),
                            /* all_done= */ base::DoNothing());
}

void PromptAction::OnAutoSelectElementExists(int choice_index, bool exists) {
  if (exists)
    auto_select_choice_index_ = choice_index;

  // Calling OnSuggestionChosen() is delayed until try_done, as it indirectly
  // deletes the batch element checker, which isn't supported from an element
  // check callback.
}

void PromptAction::OnAutoSelectDone() {
  if (auto_select_choice_index_ >= 0) {
    delegate_->CancelPrompt();
    OnSuggestionChosen(auto_select_choice_index_);
  }
}

void PromptAction::OnSuggestionChosen(int choice_index) {
  if (!callback_) {
    NOTREACHED();
    return;
  }
  DCHECK(choice_index >= 0 && choice_index <= proto_.prompt().choices_size());

  // Interrupt checks.
  precondition_checker_.reset();
  auto_select_checker_.reset();

  PromptProto::Choice choice;
  UpdateProcessedAction(ACTION_APPLIED);
  *processed_action_proto_->mutable_prompt_choice() =
      proto_.prompt().choices(choice_index);
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
