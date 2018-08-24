// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_

#include <string>

namespace autofill_assistant {
// Data shared between scripts and actions.
struct ClientMemory {
  // GUID of the currently selected credit card or empty.
  std::string selected_credit_card;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
