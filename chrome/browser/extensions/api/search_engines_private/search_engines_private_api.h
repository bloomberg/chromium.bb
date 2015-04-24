// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.searchEnginesPrivate.getDefaultSearchEngines method.
class SearchEnginesPrivateGetDefaultSearchEnginesFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateGetDefaultSearchEnginesFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.getDefaultSearchEngines",
                             SEARCHENGINESPRIVATE_GETDEFAULTSEARCHENGINES);

 protected:
  ~SearchEnginesPrivateGetDefaultSearchEnginesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateGetDefaultSearchEnginesFunction);
};

// Implements the chrome.searchEnginesPrivate.setSelectedSearchEngine method.
class SearchEnginesPrivateSetSelectedSearchEngineFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateSetSelectedSearchEngineFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.setSelectedSearchEngine",
                             SEARCHENGINESPRIVATE_SETSELECTEDSEARCHENGINE);

 protected:
  ~SearchEnginesPrivateSetSelectedSearchEngineFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateSetSelectedSearchEngineFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_API_H_
