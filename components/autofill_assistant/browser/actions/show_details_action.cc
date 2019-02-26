// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_details_action.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

ShowDetailsAction::ShowDetailsAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_show_details());
}

ShowDetailsAction::~ShowDetailsAction() {}

void ShowDetailsAction::InternalProcessAction(ActionDelegate* delegate,
                                              ProcessActionCallback callback) {
  callback_ = std::move(callback);

  if (!proto_.show_details().has_details()) {
    delegate->ClearDetails();
    OnActionProcessed(ACTION_APPLIED);
    return;
  }

  Details details(proto_.show_details());
  delegate->SetDetails(details);

  if (!details.proto().change_flags().user_approval_required()) {
    OnActionProcessed(ACTION_APPLIED);
    return;
  }

  // Ask for user approval.

  std::string previous_status_message = delegate->GetStatusMessage();
  delegate->SetStatusMessage(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DETAILS_DIFFER));

  // Continue button.
  auto chips = std::make_unique<std::vector<Chip>>();
  chips->emplace_back();
  chips->back().text =
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_CONTINUE_BUTTON);
  chips->back().type = Chip::Type::BUTTON_FILLED_BLUE;
  chips->back().callback = base::BindOnce(
      &ShowDetailsAction::OnUserConfirmation, weak_ptr_factory_.GetWeakPtr(),
      base::Unretained(delegate), previous_status_message);

  delegate->Prompt(std::move(chips));
}

void ShowDetailsAction::OnUserConfirmation(
    ActionDelegate* delegate,
    const std::string& previous_status_message) {
  // Same details, without highlights.
  Details details(proto_.show_details());
  details.ClearChanges();
  delegate->SetDetails(details);
  // Restore status message
  delegate->SetStatusMessage(previous_status_message);
  OnActionProcessed(ACTION_APPLIED);
}

void ShowDetailsAction::OnActionProcessed(ProcessedActionStatusProto status) {
  DCHECK(callback_);

  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
