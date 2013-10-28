// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_DISCOVERY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_DISCOVERY_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class DiscoverySuggestFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.discovery.suggest",
                             EXPERIMENTAL_DISCOVERY_SUGGEST)

 protected:
  virtual ~DiscoverySuggestFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class DiscoveryRemoveSuggestionFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.discovery.removeSuggestion",
                             EXPERIMENTAL_DISCOVERY_REMOVESUGGESTION)

 protected:
  virtual ~DiscoveryRemoveSuggestionFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class DiscoveryClearAllSuggestionsFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.discovery.clearAllSuggestions",
                             EXPERIMENTAL_DISCOVERY_CLEARALLSUGGESTIONS)

 protected:
  virtual ~DiscoveryClearAllSuggestionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_DISCOVERY_API_H_
