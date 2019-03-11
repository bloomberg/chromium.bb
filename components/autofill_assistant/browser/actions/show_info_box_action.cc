// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_info_box_action.h"

#include <utility>

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/info_box.h"

namespace autofill_assistant {

ShowInfoBoxAction::ShowInfoBoxAction(const ActionProto& proto)
    : Action(proto) {}

ShowInfoBoxAction::~ShowInfoBoxAction() {}

void ShowInfoBoxAction::InternalProcessAction(ActionDelegate* delegate,
                                              ProcessActionCallback callback) {
  if (!proto_.show_info_box().has_info_box()) {
    delegate->ClearInfoBox();
  } else {
    delegate->SetInfoBox(InfoBox(proto_.show_info_box()));
  }
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
