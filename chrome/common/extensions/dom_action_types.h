// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_DOM_ACTION_TYPES_H_
#define CHROME_COMMON_EXTENSIONS_DOM_ACTION_TYPES_H_

namespace extensions {

struct DomActionType {
  // These values should not be changed. Append any additional values to the
  // end with sequential numbers.
  enum Type {
    GETTER = 0,      // For Content Script DOM manipulations
    SETTER = 1,      // For Content Script DOM manipulations
    METHOD = 2,      // For Content Script DOM manipulations
    INSERTED = 3,    // For when Content Scripts are added to pages
    XHR = 4,         // When an extension core sends an XHR
    WEBREQUEST = 5,  // When a page request is modified with the WebRequest API
    MODIFIED = 6,    // For legacy, also used as a catch-all
  };
};

}  // namespace

#endif  // CHROME_COMMON_EXTENSIONS_DOM_ACTION_TYPES_H_
