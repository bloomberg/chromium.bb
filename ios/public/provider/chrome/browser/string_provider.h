// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_STRING_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_STRING_PROVIDER_H_

#include "base/strings/string16.h"

namespace ios {

// A class that provides access to localized strings.
class StringProvider {
 public:
  StringProvider() {}
  virtual ~StringProvider() {}

  // Strings used in //ios/chrome.
  // TODO(droger): Find a long term solution for strings used in //ios/chrome.

  // Returns a generic "Done" string.
  virtual base::string16 GetDoneString() = 0;
  // Returns the product name (e.g. "Google Chrome").
  virtual base::string16 GetProductName() = 0;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_STRING_PROVIDER_H_
