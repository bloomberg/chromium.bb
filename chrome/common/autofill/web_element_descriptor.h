// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOFILL_WEB_ELEMENT_DESCRIPTOR_H_
#define CHROME_COMMON_AUTOFILL_WEB_ELEMENT_DESCRIPTOR_H_

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

#endif  // CHROME_COMMON_AUTOFILL_WEB_ELEMENT_DESCRIPTOR_H_
