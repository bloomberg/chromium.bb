// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CHIP_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CHIP_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// A structure to represent a Chip shown in the carousel.
struct Chip {
  Chip();
  Chip(const ChipProto& chip_proto);
  ~Chip();
  Chip(Chip&&);
  Chip& operator=(Chip&&);

  ChipType type = UNKNOWN_CHIP_TYPE;

  ChipIcon icon = NO_ICON;

  // Localized string to display.
  std::string text;

  // Callback triggered when the chip is tapped.
  base::OnceClosure callback;

  // Whether this chip is disabled.
  bool disabled = false;

  // Whether this chip is sticky. A sticky chip will be a candidate to be
  // displayed in the header if the peek mode of the sheet is HANDLE_HEADER.
  bool sticky = false;
};

// Guarantees that the Chip.type of all chips is set to a sensible value.
void SetDefaultChipType(std::vector<Chip>* chips);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CHIP_H_
