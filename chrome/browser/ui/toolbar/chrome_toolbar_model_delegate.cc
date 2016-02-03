// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

std::string ChromeToolbarModelDelegate::GetAcceptLanguages() const {
  Profile* profile = GetProfile();
  return profile ? profile->GetPrefs()->GetString(prefs::kAcceptLanguages)
                 : std::string();
}

base::string16 ChromeToolbarModelDelegate::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const base::string16& formatted_url) const {
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, formatted_url, ChromeAutocompleteSchemeClassifier(GetProfile()));
}

ChromeToolbarModelDelegate::ChromeToolbarModelDelegate() {}

ChromeToolbarModelDelegate::~ChromeToolbarModelDelegate() {}

content::NavigationController*
ChromeToolbarModelDelegate::GetNavigationController() const {
  // This |current_tab| can be null during the initialization of the toolbar
  // during window creation (i.e. before any tabs have been added to the
  // window).
  content::WebContents* current_tab = GetActiveWebContents();
  return current_tab ? &current_tab->GetController() : nullptr;
}

Profile* ChromeToolbarModelDelegate::GetProfile() const {
  content::NavigationController* navigation_controller =
      GetNavigationController();
  return navigation_controller ? Profile::FromBrowserContext(
                                     navigation_controller->GetBrowserContext())
                               : nullptr;
}
