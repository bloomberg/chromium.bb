// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

TriggerContext::TriggerContext(std::map<std::string, std::string> params,
                               std::string exp)
    : script_parameters(std::move(params)), experiment_ids(std::move(exp)) {}

TriggerContext::TriggerContext() = default;
TriggerContext::TriggerContext(const TriggerContext& context) = default;
TriggerContext::~TriggerContext() = default;

}  // namespace autofill_assistant
