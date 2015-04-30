// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/search_engines_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.searchEnginesPrivate.getSearchEngines method.
class SearchEnginesPrivateGetSearchEnginesFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateGetSearchEnginesFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.getSearchEngines",
                             SEARCHENGINESPRIVATE_GETSEARCHENGINES);

 protected:
  ~SearchEnginesPrivateGetSearchEnginesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateGetSearchEnginesFunction);
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

// Implements the chrome.searchEnginesPrivate.addOtherSearchEngine method.
class SearchEnginesPrivateAddOtherSearchEngineFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateAddOtherSearchEngineFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.addOtherSearchEngine",
                             SEARCHENGINESPRIVATE_ADDOTHERSEARCHENGINE);

 protected:
  ~SearchEnginesPrivateAddOtherSearchEngineFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateAddOtherSearchEngineFunction);
};

// Implements the chrome.searchEnginesPrivate.updateSearchEngine method.
class SearchEnginesPrivateUpdateSearchEngineFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateUpdateSearchEngineFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.updateSearchEngine",
                             SEARCHENGINESPRIVATE_UPDATESEARCHENGINE);

 protected:
  ~SearchEnginesPrivateUpdateSearchEngineFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateUpdateSearchEngineFunction);
};

// Implements the chrome.searchEnginesPrivate.removeSearchEngine method.
class SearchEnginesPrivateRemoveSearchEngineFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateRemoveSearchEngineFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.removeSearchEngine",
                             SEARCHENGINESPRIVATE_REMOVESEARCHENGINE);

 protected:
  ~SearchEnginesPrivateRemoveSearchEngineFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateRemoveSearchEngineFunction);
};

// Implements the chrome.searchEnginesPrivate.getHotwordState method.
class SearchEnginesPrivateGetHotwordStateFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateGetHotwordStateFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.getHotwordState",
                             SEARCHENGINESPRIVATE_GETHOTWORDSTATE);

 protected:
  ~SearchEnginesPrivateGetHotwordStateFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnAudioHistoryChecked(
      scoped_ptr<api::search_engines_private::HotwordState> state,
      const base::string16& audio_history_state,
      bool success,
      bool logging_enabled);

  ChromeExtensionFunctionDetails chrome_details_;

  // Used to get WeakPtr to self for use on the UI thread.
  base::WeakPtrFactory<SearchEnginesPrivateGetHotwordStateFunction>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateGetHotwordStateFunction);
};

// Implements the chrome.searchEnginesPrivate.optIntoHotwording method.
class SearchEnginesPrivateOptIntoHotwordingFunction
    : public UIThreadExtensionFunction {
 public:
  SearchEnginesPrivateOptIntoHotwordingFunction();
  DECLARE_EXTENSION_FUNCTION("searchEnginesPrivate.optIntoHotwording",
                             SEARCHENGINESPRIVATE_OPTINTOHOTWORDING);

 protected:
  ~SearchEnginesPrivateOptIntoHotwordingFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateOptIntoHotwordingFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_API_H_
