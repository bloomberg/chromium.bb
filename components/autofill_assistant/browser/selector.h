// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace autofill_assistant {

// A structure to represent a CSS selector.
struct Selector {
  // A sequence of CSS selectors. Any non-final CSS selector is expected to
  // arrive at a frame or an iframe, i.e. an element that contains another
  // document.
  std::vector<std::string> selectors;

  Selector();
  explicit Selector(std::vector<std::string> s);
  ~Selector();

  Selector(Selector&& other);
  Selector(const Selector& other);
  Selector& operator=(Selector&& other);
  Selector& operator=(const Selector& other);

  bool operator<(const Selector& other) const;
  bool operator==(const Selector& other) const;

  // Checks whether this selector is empty.
  bool empty() const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_
