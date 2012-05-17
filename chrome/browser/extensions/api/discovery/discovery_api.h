// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_DISCOVERY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_DISCOVERY_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class DiscoverySuggestFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.discovery.suggest");

 protected:
  virtual ~DiscoverySuggestFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class DiscoveryRemoveSuggestionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.discovery.removeSuggestion");

 protected:
  virtual ~DiscoveryRemoveSuggestionFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class DiscoveryClearAllSuggestionsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.discovery.clearAllSuggestions");

 protected:
  virtual ~DiscoveryClearAllSuggestionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_DISCOVERY_API_H_
