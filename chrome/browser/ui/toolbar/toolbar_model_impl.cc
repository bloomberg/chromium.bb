// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/google/core/browser/google_util.h"
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

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate)
    : delegate_(delegate) {
}

ToolbarModelImpl::~ToolbarModelImpl() {
}

// static
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

    case content::SECURITY_STYLE_AUTHENTICATED: {
#if defined(OS_CHROMEOS)
      policy::PolicyCertService* service =
          policy::PolicyCertServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()));
      if (service && service->UsedPolicyCertificates())
        return SECURITY_POLICY_WARNING;
#endif
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
    }
    default:
      NOTREACHED();
      return NONE;
  }
}

// ToolbarModelImpl Implementation.
base::string16 ToolbarModelImpl::GetText() const {
  base::string16 search_terms(GetSearchTerms(false));
  if (!search_terms.empty())
    return search_terms;

  if (WouldOmitURLDueToOriginChip())
    return base::string16();

  return GetFormattedURL(NULL);
}

base::string16 ToolbarModelImpl::GetFormattedURL(size_t* prefix_end) const {
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
                          net::UnescapeRule::NORMAL, NULL, prefix_end, NULL),
      ChromeAutocompleteSchemeClassifier(profile));
}

base::string16 ToolbarModelImpl::GetCorpusNameForMobile() const {
  if (!WouldPerformSearchTermReplacement(false))
    return base::string16();
  GURL url(GetURL());
  // If there is a query in the url fragment look for the corpus name there,
  // otherwise look for the corpus name in the query parameters.
  const std::string& query_str(google_util::HasGoogleSearchQueryParam(
      url.ref()) ? url.ref() : url.query());
  url::Component query(0, query_str.length()), key, value;
  const char kChipKey[] = "sboxchip";
  while (url::ExtractQueryKeyValue(query_str.c_str(), &query, &key, &value)) {
    if (key.is_nonempty() && query_str.substr(key.begin, key.len) == kChipKey) {
      return net::UnescapeAndDecodeUTF8URLComponent(
          query_str.substr(value.begin, value.len),
          net::UnescapeRule::NORMAL);
    }
  }
  return base::string16();
}

GURL ToolbarModelImpl::GetURL() const {
  const NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    const NavigationEntry* entry = navigation_controller->GetVisibleEntry();
    if (entry)
      return ShouldDisplayURL() ? entry->GetVirtualURL() : GURL();
  }

  return GURL(url::kAboutBlankURL);
}

bool ToolbarModelImpl::WouldPerformSearchTermReplacement(
    bool ignore_editing) const {
  return !GetSearchTerms(ignore_editing).empty();
}

ToolbarModel::SecurityLevel ToolbarModelImpl::GetSecurityLevel(
    bool ignore_editing) const {
  // When editing, assume no security style.
  return (input_in_progress() && !ignore_editing) ?
      NONE : GetSecurityLevelForWebContents(delegate_->GetActiveWebContents());
}

int ToolbarModelImpl::GetIcon() const {
  if (WouldPerformSearchTermReplacement(false)) {
    // The secured version of the search icon is necessary if neither the search
    // button nor origin chip are present to indicate the security state.
    return (chrome::GetDisplaySearchButtonConditions() ==
        chrome::DISPLAY_SEARCH_BUTTON_NEVER) &&
        !chrome::ShouldDisplayOriginChip() ?
            IDR_OMNIBOX_SEARCH_SECURED : IDR_OMNIBOX_SEARCH;
  }

  return GetIconForSecurityLevel(GetSecurityLevel(false));
}

int ToolbarModelImpl::GetIconForSecurityLevel(SecurityLevel level) const {
  static int icon_ids[NUM_SECURITY_LEVELS] = {
    IDR_LOCATION_BAR_HTTP,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_WARNING,
    IDR_OMNIBOX_HTTPS_POLICY_WARNING,
    IDR_OMNIBOX_HTTPS_INVALID,
  };
  DCHECK(arraysize(icon_ids) == NUM_SECURITY_LEVELS);
  return icon_ids[level];
}

base::string16 ToolbarModelImpl::GetEVCertName() const {
  if (GetSecurityLevel(false) != EV_SECURE)
    return base::string16();

  // Note: Navigation controller and active entry are guaranteed non-NULL or
  // the security level would be NONE.
  scoped_refptr<net::X509Certificate> cert;
  content::CertStore::GetInstance()->RetrieveCert(
      GetNavigationController()->GetVisibleEntry()->GetSSL().cert_id, &cert);

  // EV are required to have an organization name and country.
  DCHECK(!cert->subject().organization_names.empty());
  DCHECK(!cert->subject().country_name.empty());
  return l10n_util::GetStringFUTF16(
      IDS_SECURE_CONNECTION_EV,
      base::UTF8ToUTF16(cert->subject().organization_names[0]),
      base::UTF8ToUTF16(cert->subject().country_name));
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
    if (url.SchemeIs(content::kChromeUIScheme) ||
        virtual_url.SchemeIs(content::kChromeUIScheme)) {
      if (!url.SchemeIs(content::kChromeUIScheme))
        url = virtual_url;
      return url.host() != chrome::kChromeUINewTabHost;
    }
  }

  return !chrome::IsInstantNTP(delegate_->GetActiveWebContents());
}

bool ToolbarModelImpl::WouldOmitURLDueToOriginChip() const {
  const char kInterstitialShownKey[] = "interstitial_shown";

  // When users type URLs and hit enter, continue to show those URLs until
  // the navigation commits or an interstitial is shown, because having the
  // omnibox clear immediately feels like the input was ignored.
  NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    NavigationEntry* pending_entry = navigation_controller->GetPendingEntry();
    if (pending_entry) {
      const NavigationEntry* visible_entry =
          navigation_controller->GetVisibleEntry();
      base::string16 unused;
      // Keep track that we've shown the origin chip on an interstitial so it
      // can be shown even after the interstitial was dismissed, to avoid
      // showing the chip, removing it and then showing it again.
      if (visible_entry &&
          visible_entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL &&
          !pending_entry->GetExtraData(kInterstitialShownKey, &unused))
        pending_entry->SetExtraData(kInterstitialShownKey, base::string16());
      const content::PageTransition transition_type =
          pending_entry->GetTransitionType();
      if ((transition_type & content::PAGE_TRANSITION_TYPED) != 0 &&
          !pending_entry->GetExtraData(kInterstitialShownKey, &unused))
        return false;
    }
  }

  if (!delegate_->InTabbedBrowser() || !ShouldDisplayURL() ||
      !url_replacement_enabled())
    return false;

  if (chrome::ShouldDisplayOriginChip())
    return true;

  const chrome::OriginChipCondition chip_condition =
      chrome::GetOriginChipCondition();
  return (chip_condition == chrome::ORIGIN_CHIP_ALWAYS) ||
      ((chip_condition == chrome::ORIGIN_CHIP_ON_SRP) &&
       WouldPerformSearchTermReplacement(false));
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

base::string16 ToolbarModelImpl::GetSearchTerms(bool ignore_editing) const {
  if (!url_replacement_enabled() || (input_in_progress() && !ignore_editing))
    return base::string16();

  const WebContents* web_contents = delegate_->GetActiveWebContents();
  base::string16 search_terms(chrome::GetSearchTerms(web_contents));
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
  const NavigationController& nav_controller = web_contents->GetController();
  const NavigationEntry* entry = nav_controller.GetVisibleEntry();
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
  ToolbarModel::SecurityLevel security_level = GetSecurityLevel(ignore_editing);
  return ((security_level == NONE) || (security_level == SECURITY_ERROR)) ?
      base::string16() : search_terms;
}
