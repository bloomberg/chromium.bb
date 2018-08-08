// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_web_controller.h"

#include "base/callback.h"

namespace autofill_assistant {

AssistantWebController::AssistantWebController() {}

AssistantWebController::~AssistantWebController() {}

void AssistantWebController::ClickElement(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement click operation.
  std::move(callback).Run(true);
}

void AssistantWebController::ElementExists(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement check existence operation.
  std::move(callback).Run(true);
}

}  // namespace autofill_assistant.
