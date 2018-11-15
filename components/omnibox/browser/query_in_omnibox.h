// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_QUERY_IN_OMNIBOX_H_
#define COMPONENTS_OMNIBOX_BROWSER_QUERY_IN_OMNIBOX_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

class AutocompleteClassifier;
class TemplateURLService;

// This class holds the business logic for Query in Omnibox shared between both
// Android and Desktop.
class QueryInOmnibox : public KeyedService {
 public:
  QueryInOmnibox(AutocompleteClassifier* autocomplete_classifier,
                 TemplateURLService* template_url_service);

  // Returns true if the toolbar should display the search terms. When this
  // method returns true, the extracted search terms will be filled into
  // |search_terms| if its not nullptr. |ignore_security_level| can be set to
  // true to avoid a flicker during page loading for a search results page
  // before the SSL state updates.
  //
  // This method will return false if any of the following are true:
  //  - Query in Omnibox is disabled
  //  - |security_level| is insufficient to show search terms instead of the URL
  //    and |ignore_security_level| is false.
  //  - |url| is not from the default search provider
  //  - There are no search terms to extract from |url|
  //  - The extracted search terms look too much like a URL (could confuse user)
  //
  // This method can be called with nullptr |search_terms| if the caller wants
  // to check the display status only. Virtual for testing purposes.
  virtual bool GetDisplaySearchTerms(
      security_state::SecurityLevel security_level,
      bool ignore_security_level,
      const GURL& url,
      base::string16* search_terms);

 protected:
  // For testing only.
  QueryInOmnibox();

 private:
  // Extracts search terms from |url|. Returns an empty string if |url| is not
  // from the default search provider, if there are no search terms in |url|,
  // or if the extracted search terms look too much like a URL.
  base::string16 ExtractSearchTermsInternal(const GURL& url);

  // Because extracting search terms from a URL string is relatively expensive,
  // and we want to support cheap calls to GetDisplaySearchTerms, cache the
  // result of the last-parsed URL string.
  base::string16 cached_search_terms_;
  GURL cached_url_;

  // Non-owning weak pointers.
  AutocompleteClassifier* const autocomplete_classifier_;
  TemplateURLService* const template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(QueryInOmnibox);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_QUERY_IN_OMNIBOX_H_
