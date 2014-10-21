// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_IMPL_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/omnibox/omnibox_current_page_delegate.h"

class OmniboxEditController;
class Profile;

class OmniboxCurrentPageDelegateImpl : public OmniboxCurrentPageDelegate {
 public:
  explicit OmniboxCurrentPageDelegateImpl(OmniboxEditController* controller,
                                          Profile* profile);
  ~OmniboxCurrentPageDelegateImpl() override;

  // OmniboxCurrentPageDelegate.
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  bool IsInstantNTP() const override;
  bool IsSearchResultsPage() const override;
  bool IsLoading() const override;
  content::NavigationController& GetNavigationController() const override;
  const SessionID& GetSessionID() const override;
  bool ProcessExtensionKeyword(TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition) override;
  void OnInputStateChanged() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void DoPrerender(const AutocompleteMatch& match) override;
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion) override;

 private:
  OmniboxEditController* controller_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxCurrentPageDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_IMPL_H_
