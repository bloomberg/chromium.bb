// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/ssl_status.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate)
    : delegate_(delegate),
      input_in_progress_(false),
      supports_extraction_of_url_like_search_terms_(false) {
}

ToolbarModelImpl::~ToolbarModelImpl() {
}

ToolbarModel::SecurityLevel ToolbarModelImpl::GetSecurityLevelForWebContents(
      content::WebContents* web_contents) {
  if (!web_contents)
    return NONE;

  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return NONE;

  const SSLStatus& ssl = entry->GetSSL();
  switch (ssl.security_style) {
    case content::SECURITY_STYLE_UNKNOWN:
    case content::SECURITY_STYLE_UNAUTHENTICATED:
      return NONE;

    case content::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return SECURITY_ERROR;

    case content::SECURITY_STYLE_AUTHENTICATED:
      if (policy::ProfilePolicyConnectorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))->
          UsedPolicyCertificates())
        return SECURITY_POLICY_WARNING;
      if (!!(ssl.content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT))
        return SECURITY_WARNING;
      if (net::IsCertStatusError(ssl.cert_status)) {
        DCHECK(net::IsCertStatusMinorError(ssl.cert_status));
        return SECURITY_WARNING;
      }
      if ((ssl.cert_status & net::CERT_STATUS_IS_EV) &&
          content::CertStore::GetInstance()->RetrieveCert(ssl.cert_id, NULL))
        return EV_SECURE;
      return SECURE;

    default:
      NOTREACHED();
      return NONE;
  }
}

// ToolbarModelImpl Implementation.
string16 ToolbarModelImpl::GetText(
    bool display_search_urls_as_search_terms) const {
  if (display_search_urls_as_search_terms) {
    string16 search_terms(
        chrome::GetSearchTerms(delegate_->GetActiveWebContents()));
    if (GetSearchTermsTypeInternal(search_terms) != NO_SEARCH_TERMS)
      return search_terms;
  }
  std::string languages;  // Empty if we don't have a |navigation_controller|.
  Profile* profile = GetProfile();
  if (profile)
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

  GURL url(GetURL());
  if (url.spec().length() > content::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.GetOrigin() : GURL(url.scheme() + ":");
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, net::FormatUrl(url, languages, net::kFormatUrlOmitAll,
                          net::UnescapeRule::NORMAL, NULL, NULL, NULL));
}

string16 ToolbarModelImpl::GetCorpusNameForMobile() const {
  if (GetSearchTermsType() == NO_SEARCH_TERMS)
    return string16();
  GURL url(GetURL());
  // If there is a query in the url fragment look for the corpus name there,
  // otherwise look for the corpus name in the query parameters.
  const std::string& query_str(google_util::HasGoogleSearchQueryParam(
      url.ref()) ? url.ref() : url.query());
  url_parse::Component query(0, query_str.length()), key, value;
  const char kChipKey[] = "sboxchip";
  while (url_parse::ExtractQueryKeyValue(query_str.c_str(), &query, &key,
                                         &value)) {
    if (key.is_nonempty() && query_str.substr(key.begin, key.len) == kChipKey) {
      return net::UnescapeAndDecodeUTF8URLComponent(
          query_str.substr(value.begin, value.len),
          net::UnescapeRule::NORMAL, NULL);
    }
  }
  return string16();
}

GURL ToolbarModelImpl::GetURL() const {
  const NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    const NavigationEntry* entry = navigation_controller->GetVisibleEntry();
    if (entry)
      return ShouldDisplayURL() ? entry->GetVirtualURL() : GURL();
  }

  return GURL(chrome::kAboutBlankURL);
}

ToolbarModel::SearchTermsType ToolbarModelImpl::GetSearchTermsType() const {
  return GetSearchTermsTypeInternal(
      chrome::GetSearchTerms(delegate_->GetActiveWebContents()));
}

void ToolbarModelImpl::SetSupportsExtractionOfURLLikeSearchTerms(bool value) {
  supports_extraction_of_url_like_search_terms_ = value;
}

bool ToolbarModelImpl::ShouldDisplayURL() const {
  // Note: The order here is important.
  // - The WebUI test must come before the extension scheme test because there
  //   can be WebUIs that have extension schemes (e.g. the bookmark manager). In
  //   that case, we should prefer what the WebUI instance says.
  // - The view-source test must come before the NTP test because of the case
  //   of view-source:chrome://newtab, which should display its URL despite what
  //   chrome://newtab says.
  NavigationController* controller = GetNavigationController();
  NavigationEntry* entry = controller ? controller->GetVisibleEntry() : NULL;
  if (entry) {
    if (entry->IsViewSourceMode() ||
        entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL) {
      return true;
    }

    GURL url = entry->GetURL();
    GURL virtual_url = entry->GetVirtualURL();
    if (url.SchemeIs(chrome::kChromeUIScheme) ||
        virtual_url.SchemeIs(chrome::kChromeUIScheme)) {
      if (!url.SchemeIs(chrome::kChromeUIScheme))
        url = virtual_url;
      return url.host() != chrome::kChromeUINewTabHost;
    }
  }

  if (chrome::IsInstantNTP(delegate_->GetActiveWebContents()))
    return false;

  return true;
}

ToolbarModel::SecurityLevel ToolbarModelImpl::GetSecurityLevel() const {
  if (input_in_progress_)  // When editing, assume no security style.
    return NONE;

  return GetSecurityLevelForWebContents(delegate_->GetActiveWebContents());
}

int ToolbarModelImpl::GetIcon() const {
  SearchTermsType search_terms_type = GetSearchTermsType();
  SecurityLevel security_level = GetSecurityLevel();
  if (search_terms_type == NORMAL_SEARCH_TERMS ||
      (search_terms_type == URL_LIKE_SEARCH_TERMS && IsSearchPageSecure()))
    return IDR_OMNIBOX_SEARCH;

  static int icon_ids[NUM_SECURITY_LEVELS] = {
    IDR_LOCATION_BAR_HTTP,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_WARNING,
    IDR_OMNIBOX_HTTPS_POLICY_WARNING,
    IDR_OMNIBOX_HTTPS_INVALID,
  };
  DCHECK(arraysize(icon_ids) == NUM_SECURITY_LEVELS);
  return icon_ids[security_level];
}

string16 ToolbarModelImpl::GetEVCertName() const {
  DCHECK_EQ(GetSecurityLevel(), EV_SECURE);
  scoped_refptr<net::X509Certificate> cert;
  // Note: Navigation controller and active entry are guaranteed non-NULL or
  // the security level would be NONE.
  content::CertStore::GetInstance()->RetrieveCert(
      GetNavigationController()->GetVisibleEntry()->GetSSL().cert_id, &cert);
  return GetEVCertName(*cert);
}

// static
string16 ToolbarModelImpl::GetEVCertName(const net::X509Certificate& cert) {
  // EV are required to have an organization name and country.
  if (cert.subject().organization_names.empty() ||
      cert.subject().country_name.empty()) {
    NOTREACHED();
    return string16();
  }

  return l10n_util::GetStringFUTF16(
      IDS_SECURE_CONNECTION_EV,
      UTF8ToUTF16(cert.subject().organization_names[0]),
      UTF8ToUTF16(cert.subject().country_name));
}

void ToolbarModelImpl::SetInputInProgress(bool value) {
  input_in_progress_ = value;
}

bool ToolbarModelImpl::GetInputInProgress() const {
  return input_in_progress_;
}

NavigationController* ToolbarModelImpl::GetNavigationController() const {
  // This |current_tab| can be NULL during the initialization of the
  // toolbar during window creation (i.e. before any tabs have been added
  // to the window).
  WebContents* current_tab = delegate_->GetActiveWebContents();
  return current_tab ? &current_tab->GetController() : NULL;
}

Profile* ToolbarModelImpl::GetProfile() const {
  NavigationController* navigation_controller = GetNavigationController();
  return navigation_controller ?
      Profile::FromBrowserContext(navigation_controller->GetBrowserContext()) :
      NULL;
}

ToolbarModel::SearchTermsType ToolbarModelImpl::GetSearchTermsTypeInternal(
    const string16& search_terms) const {
  if (search_terms.empty())
    return NO_SEARCH_TERMS;

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(Profile::FromBrowserContext(
      delegate_->GetActiveWebContents()->GetBrowserContext()))->Classify(
          search_terms, false, false, &match, NULL);
  // If the current page is not being displayed securely (e.g. due to mixed
  // content errors), force any extracted search terms to be considered as
  // "URL-like".  This will cause callers to display a more obvious indicator
  // that the terms are a search query.  This aims to mitigate an attack
  // scenario where attackers inject content into a search result page to make
  // it look like e.g. Paypal and also navigate the URL to that of a search for
  // "Paypal" -- hopefully, showing an obvious indicator that this is a search
  // will decrease the chances of users falling for the phishing attempt.
  if (AutocompleteMatch::IsSearchType(match.type) && IsSearchPageSecure())
    return NORMAL_SEARCH_TERMS;

  return supports_extraction_of_url_like_search_terms_?
      URL_LIKE_SEARCH_TERMS : NO_SEARCH_TERMS;
}

bool ToolbarModelImpl::IsSearchPageSecure() const {
  WebContents* web_contents = delegate_->GetActiveWebContents();
  // This method should only be called for search pages.
  DCHECK(!chrome::GetSearchTerms(web_contents).empty());

  const NavigationController& nav_controller = web_contents->GetController();

  // If the page is still loading and the security style is unkown then consider
  // the page secure.
  if (nav_controller.GetVisibleEntry()->GetSSL().security_style ==
      content::SECURITY_STYLE_UNKNOWN && nav_controller.GetVisibleEntry() !=
                                         nav_controller.GetLastCommittedEntry())
    return true;

  ToolbarModel::SecurityLevel security_level = GetSecurityLevel();
  return security_level == SECURE || security_level == EV_SECURE;
}
