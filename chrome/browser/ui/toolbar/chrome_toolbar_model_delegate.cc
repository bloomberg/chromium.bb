// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"

#include "base/logging.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"

ChromeToolbarModelDelegate::ChromeToolbarModelDelegate() {}

ChromeToolbarModelDelegate::~ChromeToolbarModelDelegate() {}

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

bool ChromeToolbarModelDelegate::GetURL(GURL* url) const {
  DCHECK(url);
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry)
    return false;

  *url = ShouldDisplayURL() ? entry->GetVirtualURL() : GURL();
  return true;
}

bool ChromeToolbarModelDelegate::ShouldDisplayURL() const {
  // Note: The order here is important.
  // - The WebUI test must come before the extension scheme test because there
  //   can be WebUIs that have extension schemes (e.g. the bookmark manager). In
  //   that case, we should prefer what the WebUI instance says.
  // - The view-source test must come before the NTP test because of the case
  //   of view-source:chrome://newtab, which should display its URL despite what
  //   chrome://newtab says.
  content::NavigationEntry* entry = GetNavigationEntry();
  if (entry) {
    if (entry->IsViewSourceMode() ||
        entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL) {
      return true;
    }

    GURL url = entry->GetURL();
    GURL virtual_url = entry->GetVirtualURL();
    if (url.SchemeIs(content::kChromeUIScheme) ||
        virtual_url.SchemeIs(content::kChromeUIScheme)) {
      if (!url.SchemeIs(content::kChromeUIScheme))
        url = virtual_url;
      return url.host() != chrome::kChromeUINewTabHost;
    }
  }

  return !search::IsInstantNTP(GetActiveWebContents());
}

security_state::SecurityStateModel::SecurityLevel
ChromeToolbarModelDelegate::GetSecurityLevel() const {
  content::WebContents* web_contents = GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), assume no security style.
  if (!web_contents)
    return security_state::SecurityStateModel::NONE;
  auto* client = ChromeSecurityStateModelClient::FromWebContents(web_contents);
  return client->GetSecurityInfo().security_level;
}

base::string16 ChromeToolbarModelDelegate::GetSearchTerms(
    security_state::SecurityStateModel::SecurityLevel security_level) const {
  content::WebContents* web_contents = GetActiveWebContents();
  base::string16 search_terms(search::GetSearchTerms(web_contents));
  if (search_terms.empty()) {
    // We mainly do this to enforce the subsequent DCHECK.
    return base::string16();
  }

  // If the page is still loading and the security style is unknown, consider
  // the page secure.  Without this, after the user hit enter on some search
  // terms, the omnibox would change to displaying the loading URL before
  // changing back to the search terms once they could be extracted, thus
  // causing annoying flicker.
  DCHECK(web_contents);
  content::NavigationController& nav_controller = web_contents->GetController();
  content::NavigationEntry* entry = nav_controller.GetVisibleEntry();
  if ((entry != nav_controller.GetLastCommittedEntry()) &&
      (entry->GetSSL().security_style == content::SECURITY_STYLE_UNKNOWN))
    return search_terms;

  // If the URL is using a Google base URL specified via the command line, we
  // bypass the security check below.
  if (entry &&
      google_util::StartsWithCommandLineGoogleBaseURL(entry->GetVirtualURL()))
    return search_terms;

  // Otherwise, extract search terms for HTTPS pages that do not have a security
  // error.
  bool extract_search_terms =
      (security_level != security_state::SecurityStateModel::NONE) &&
      (security_level != security_state::SecurityStateModel::SECURITY_ERROR);
  return extract_search_terms ? search_terms : base::string16();
}

scoped_refptr<net::X509Certificate> ChromeToolbarModelDelegate::GetCertificate()
    const {
  scoped_refptr<net::X509Certificate> cert;
  content::NavigationEntry* entry = GetNavigationEntry();
  if (entry) {
    content::CertStore::GetInstance()->RetrieveCert(entry->GetSSL().cert_id,
                                                    &cert);
  }
  return cert;
}

content::NavigationController*
ChromeToolbarModelDelegate::GetNavigationController() const {
  // This |current_tab| can be null during the initialization of the toolbar
  // during window creation (i.e. before any tabs have been added to the
  // window).
  content::WebContents* current_tab = GetActiveWebContents();
  return current_tab ? &current_tab->GetController() : nullptr;
}

content::NavigationEntry* ChromeToolbarModelDelegate::GetNavigationEntry()
    const {
  content::NavigationController* controller = GetNavigationController();
  return controller ? controller->GetVisibleEntry() : nullptr;
}

Profile* ChromeToolbarModelDelegate::GetProfile() const {
  content::NavigationController* controller = GetNavigationController();
  return controller
             ? Profile::FromBrowserContext(controller->GetBrowserContext())
             : nullptr;
}
