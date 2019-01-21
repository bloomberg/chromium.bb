// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CHIP_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CHIP_H_

#include <string>

#include "base/callback.h"

namespace autofill_assistant {

// A structure to represent a Chip shown in the carousel.
struct Chip {
  Chip();
  ~Chip();
  Chip(Chip&&);
  Chip& operator=(Chip&&);

  // The type a chip can have. The terminology is borrow from:
  //  - https://guidelines.googleplex.com/googlematerial/components/chips.html
  //  - https://guidelines.googleplex.com/googlematerial/components/buttons.html
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.carousel)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantChipType
  enum Type {
    CHIP_ASSISTIVE = 0,
    BUTTON_FILLED_BLUE = 1,
    BUTTON_TEXT = 2,
  };

  // The type of the chip.
  Type type;

  // Localized string to display.
  std::string text;

  // Callback triggered when the chip is tapped.
  base::OnceClosure callback;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CHIP_H_
