// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "ui/app_list/search_provider.h"

class AutocompleteController;
class AutocompleteResult;
class Profile;

namespace app_list {

// OmniboxProvider wraps AutocompleteController to provide omnibox results.
class OmniboxProvider : public SearchProvider,
                        public AutocompleteControllerDelegate {
 public:
  explicit OmniboxProvider(Profile* profile);
  virtual ~OmniboxProvider();

  // SearchProvider overrides:
  virtual void Start(const base::string16& query) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // Populates result list from AutocompleteResult.
  void PopulateFromACResult(const AutocompleteResult& result);

  // AutocompleteControllerDelegate overrides:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  Profile* profile_;

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  scoped_ptr<AutocompleteController> controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_
