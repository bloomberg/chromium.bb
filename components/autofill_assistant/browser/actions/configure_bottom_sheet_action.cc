// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/configure_bottom_sheet_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ConfigureBottomSheetAction::ConfigureBottomSheetAction(const ActionProto& proto)
    : Action(proto) {}

ConfigureBottomSheetAction::~ConfigureBottomSheetAction() {}

void ConfigureBottomSheetAction::InternalProcessAction(
    ActionDelegate* delegate,
    ProcessActionCallback callback) {
  if (proto_.configure_bottom_sheet().viewport_resizing() ==
      ConfigureBottomSheetProto::RESIZE) {
    delegate->SetResizeViewport(true);
  } else if (proto_.configure_bottom_sheet().viewport_resizing() ==
             ConfigureBottomSheetProto::NO_RESIZE) {
    delegate->SetResizeViewport(false);
  }

  if (proto_.configure_bottom_sheet().peek_mode() !=
      ConfigureBottomSheetProto::UNDEFINED_PEEK_MODE) {
    delegate->SetPeekMode(proto_.configure_bottom_sheet().peek_mode());
  }

  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
