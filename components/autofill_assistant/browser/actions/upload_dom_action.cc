// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UploadDomAction::UploadDomAction(const ActionProto& proto) : Action(proto) {
  DCHECK(proto_.has_upload_dom());
}

UploadDomAction::~UploadDomAction() {}

void UploadDomAction::ProcessAction(ActionDelegate* delegate,
                                    ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  // We return a dummy dom tree for now.
  NodeProto* root_node = processed_action_proto_->mutable_page_content()
                             ->mutable_dom_tree()
                             ->mutable_root();
  root_node->set_type(NodeProto::ELEMENT);
  root_node->set_value("BODY");
  UpdateProcessedAction(true);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant.
