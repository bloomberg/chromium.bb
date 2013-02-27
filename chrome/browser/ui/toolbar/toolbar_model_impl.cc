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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ui/search/search.h"
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
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_util.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate)
    : delegate_(delegate),
      input_in_progress_(false) {
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
    string16 search_terms = GetSearchTerms();
    if (!search_terms.empty())
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

GURL ToolbarModelImpl::GetURL() const {
  const NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    const NavigationEntry* entry = navigation_controller->GetVisibleEntry();
    if (entry)
      return ShouldDisplayURL() ? entry->GetVirtualURL() : GURL();
  }

  return GURL(chrome::kAboutBlankURL);
}

bool ToolbarModelImpl::WouldReplaceSearchURLWithSearchTerms() const {
  return !GetSearchTerms().empty();
}

bool ToolbarModelImpl::ShouldDisplayURL() const {
  // Note: The order here is important.
  // - The WebUI test must come before the extension scheme test because there
  //   can be WebUIs that have extension schemes (e.g. the bookmark manager). In
  //   that case, we should prefer what the WebUI instance says.
  // - The view-source test must come before the WebUI test because of the case
  //   of view-source:chrome://newtab, which should display its URL despite what
  //   chrome://newtab's WebUI says.
  NavigationController* controller = GetNavigationController();
  NavigationEntry* entry = controller ? controller->GetVisibleEntry() : NULL;
  if (entry) {
    if (entry->IsViewSourceMode() ||
        entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL) {
      return true;
    }
  }

  WebContents* web_contents = delegate_->GetActiveWebContents();
  if (web_contents && web_contents->GetWebUIForCurrentState())
    return !web_contents->GetWebUIForCurrentState()->ShouldHideURL();

  if (entry && entry->GetURL().SchemeIs(extensions::kExtensionScheme))
    return false;

#if defined(OS_CHROMEOS)
  if (entry && entry->GetURL().SchemeIs(chrome::kDriveScheme))
    return false;
#endif

  if (entry && entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL))
    return false;

  return true;
}

ToolbarModel::SecurityLevel ToolbarModelImpl::GetSecurityLevel() const {
  if (input_in_progress_)  // When editing, assume no security style.
    return NONE;

  return GetSecurityLevelForWebContents(delegate_->GetActiveWebContents());
}

int ToolbarModelImpl::GetIcon() const {
  if (WouldReplaceSearchURLWithSearchTerms())
    return IDR_OMNIBOX_SEARCH;
  static int icon_ids[NUM_SECURITY_LEVELS] = {
    IDR_LOCATION_BAR_HTTP,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_WARNING,
    IDR_OMNIBOX_HTTPS_INVALID,
  };
  DCHECK(arraysize(icon_ids) == NUM_SECURITY_LEVELS);
  return icon_ids[GetSecurityLevel()];
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

string16 ToolbarModelImpl::GetSearchTerms() const {
  const WebContents* contents = delegate_->GetActiveWebContents();
  string16 search_terms = chrome::search::GetSearchTerms(contents);

  // Don't extract search terms that the omnibox would treat as a navigation.
  // This might confuse users into believing that the search terms were the
  // URL of the current page, and could cause problems if users hit enter in
  // the omnibox expecting to reload the page.
  if (!search_terms.empty()) {
    AutocompleteMatch match;
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    AutocompleteClassifierFactory::GetForProfile(profile)->Classify(
        search_terms, string16(), false, false, &match, NULL);
    if (!AutocompleteMatch::IsSearchType(match.type))
      search_terms.clear();
  }

  return search_terms;
}
