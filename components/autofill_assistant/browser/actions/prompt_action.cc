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
#include "url/gurl.h"

namespace autofill_assistant {

PromptAction::PromptAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_prompt());
}

PromptAction::~PromptAction() {}

void PromptAction::InternalProcessAction(ActionDelegate* delegate,
                                         ProcessActionCallback callback) {
  delegate->SetStatusMessage(proto_.prompt().message());

  callback_ = std::move(callback);
  DCHECK_GT(proto_.prompt().choices_size(), 0);

  delegate->Prompt(CreateChips());

  batch_element_checker_ = delegate->CreateBatchElementChecker();

  // Register elements whose existence enable new chips.
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    Selector selector(choice_proto.show_only_if_element_exists());
    if (selector.empty())
      continue;

    batch_element_checker_->AddElementCheck(
        kExistenceCheck, selector,
        base::BindOnce(&PromptAction::OnRequiredElementExists,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(delegate), i));
  }

  // Wait as long as necessary for one of the elements to show up. This is
  // cancelled by CancelProto()
  for (const auto& choice_proto : proto_.prompt().choices()) {
    Selector selector(choice_proto.element_exists());
    if (selector.empty())
      continue;

    std::string payload;
    choice_proto.SerializeToString(&payload);
    batch_element_checker_->AddElementCheck(
        kExistenceCheck, selector,
        base::BindOnce(&PromptAction::OnElementExist,
                       weak_ptr_factory_.GetWeakPtr(), payload));
  }

  batch_element_checker_->Run(
      base::TimeDelta::Max(),
      /* try_done= */
      base::BindRepeating(&PromptAction::OnElementChecksDone,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Unretained(delegate)),
      /* all_done= */ base::DoNothing());
}

std::unique_ptr<std::vector<Chip>> PromptAction::CreateChips() {
  auto chips = std::make_unique<std::vector<Chip>>();
  // TODO(crbug.com/806868): Surface type in proto instead of guessing it from
  // highlight flag.
  Chip::Type non_highlight_type = Chip::Type::CHIP_ASSISTIVE;
  for (const auto& choice_proto : proto_.prompt().choices()) {
    if (!choice_proto.name().empty() && choice_proto.highlight()) {
      non_highlight_type = Chip::Type::BUTTON_TEXT;
      break;
    }
  }

  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    if (choice_proto.show_only_if_element_exists().selectors_size() > 0 &&
        required_element_found_.count(i) == 0)
      continue;

    chips->emplace_back();
    chips->back().type = choice_proto.highlight()
                             ? Chip::Type::BUTTON_FILLED_BLUE
                             : non_highlight_type;
    chips->back().text = choice_proto.name();

    std::string server_payload;
    choice_proto.SerializeToString(&server_payload);

    chips->back().callback =
        base::BindOnce(&PromptAction::OnSuggestionChosen,
                       weak_ptr_factory_.GetWeakPtr(), server_payload);
  }
  return chips;
}

void PromptAction::OnElementExist(const std::string& payload, bool exists) {
  if (exists)
    forced_payload_ = payload;

  // Forcing the chip click is delayed until try_done, as it indirectly deletes
  // batch_element_checker_, which isn't supported from an element check
  // callback.
}

void PromptAction::OnRequiredElementExists(ActionDelegate* delegate,
                                           int choice_index,
                                           bool exists) {
  if (!exists)
    return;

  required_element_found_.insert(choice_index);
  delegate->Prompt(CreateChips());
}

void PromptAction::OnElementChecksDone(ActionDelegate* delegate) {
  if (!forced_payload_.empty()) {
    delegate->CancelPrompt();
    OnSuggestionChosen(forced_payload_);
  }
}

void PromptAction::OnSuggestionChosen(const std::string& payload) {
  batch_element_checker_.reset();
  PromptProto::Choice choice;
  if (!choice.ParseFromString(payload)) {
    DVLOG(1) << "Invalid result.";
    UpdateProcessedAction(OTHER_ACTION_STATUS);
  } else {
    UpdateProcessedAction(ACTION_APPLIED);
    std::swap(*processed_action_proto_->mutable_prompt_choice(), choice);
  }
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
