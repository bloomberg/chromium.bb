// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_TYPES_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_TYPES_H_

#include <string>

namespace safe_browsing {

// Used to store the name and value of an HTML Element's attribute.
using AttributeNameValue = std::pair<std::string, std::string>;
}

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_TYPES_H_
