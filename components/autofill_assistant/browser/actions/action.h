// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class ActionDelegate;

// An action that performs a single step of a script on the website.
class Action {
 public:
  virtual ~Action();

  // Callback runs after this action is executed, pass the result of this action
  // through a ProcessedActionProto object.
  // Delegate should outlive this object.
  using ProcessActionCallback =
      base::OnceCallback<void(std::unique_ptr<ProcessedActionProto>)>;
  // ProcessAction should create a processed_action_proto_ to be passed to the
  // callback.
  virtual void ProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) = 0;

  const ActionProto& proto() const { return proto_; }

 protected:
  explicit Action(const ActionProto& proto);
  void UpdateProcessedAction(ProcessedActionStatusProto status);

  const ActionProto proto_;

  // Accumulate any result of this action during ProcessAction. Is only valid
  // during a run of ProcessAction.
  std::unique_ptr<ProcessedActionProto> processed_action_proto_;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
