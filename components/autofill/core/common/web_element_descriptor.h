// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_WEB_ELEMENT_DESCRIPTOR_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_WEB_ELEMENT_DESCRIPTOR_H_

#include <string>

namespace autofill {

// Holds information that can be used to retrieve an element.
struct WebElementDescriptor {
  enum RetrievalMethod {
    CSS_SELECTOR,
    ID,
    NONE,
  };

  WebElementDescriptor();

  // Information to retrieve element with.
  std::string descriptor;

  // Which retrieval method to use.
  RetrievalMethod retrieval_method;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_WEB_ELEMENT_DESCRIPTOR_H_
