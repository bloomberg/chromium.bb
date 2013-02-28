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
  virtual ~OmniboxCurrentPageDelegateImpl();

  // OmniboxCurrentPageDelegate.
  virtual bool CurrentPageExists() const OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;
  virtual content::NavigationController&
      GetNavigationController() const OVERRIDE;
  virtual const SessionID& GetSessionID() const OVERRIDE;
  virtual bool ProcessExtensionKeyword(TemplateURL* template_url,
                                       const AutocompleteMatch& match) OVERRIDE;
  virtual void NotifySearchTabHelper(bool user_input_in_progress,
                                     bool cancelling) OVERRIDE;
  virtual void DoPrerender(const AutocompleteMatch& match) OVERRIDE;

 private:
  OmniboxEditController* controller_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxCurrentPageDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_IMPL_H_
