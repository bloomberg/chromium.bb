// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_details_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ShowDetailsAction::ShowDetailsAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_show_details());
}

ShowDetailsAction::~ShowDetailsAction() {}

void ShowDetailsAction::InternalProcessAction(ActionDelegate* delegate,
                                              ProcessActionCallback callback) {
  if (!proto_.show_details().has_details()) {
    delegate->HideDetails();
    UpdateProcessedAction(ACTION_APPLIED);
    std::move(callback).Run(std::move(processed_action_proto_));
  } else {
    delegate->ShowDetails(proto_.show_details().details(),
                          base::BindOnce(&ShowDetailsAction::OnShowDetails,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback), delegate));
  }
}

void ShowDetailsAction::OnShowDetails(ProcessActionCallback callback,
                                      ActionDelegate* delegate,
                                      bool can_continue) {
  if (!can_continue) {
    delegate->Close();
  }
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
