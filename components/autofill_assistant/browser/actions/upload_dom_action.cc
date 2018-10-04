// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UploadDomAction::UploadDomAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_upload_dom());
}

UploadDomAction::~UploadDomAction() {}

void UploadDomAction::ProcessAction(ActionDelegate* delegate,
                                    ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();

  std::vector<std::string> selectors;
  for (const auto& selector : proto_.upload_dom().tree_root().selectors()) {
    selectors.emplace_back(selector);
  }
  NodeProto* root_node = processed_action_proto_->mutable_page_content()
                             ->mutable_dom_tree()
                             ->mutable_root();
  delegate->BuildNodeTree(
      selectors, root_node,
      base::BindOnce(&UploadDomAction::OnBuildNodeTree,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UploadDomAction::OnBuildNodeTree(ProcessActionCallback callback,
                                      bool status) {
  // TODO(crbug.com/806868): Distinguish element not found from other error and
  // report them as ELEMENT_RESOLUTION_FAILED.
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
