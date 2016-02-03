// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/security_state_model.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/ssl_status.h"
#include "grit/components_scaled_resources.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icons_public.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;
using security_state::SecurityStateModel;

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate)
    : delegate_(delegate) {
}

ToolbarModelImpl::~ToolbarModelImpl() {
}

// ToolbarModelImpl Implementation.
base::string16 ToolbarModelImpl::GetText() const {
  base::string16 search_terms(GetSearchTerms(false));
  if (!search_terms.empty())
    return search_terms;

  return GetFormattedURL(NULL);
}

base::string16 ToolbarModelImpl::GetFormattedURL(size_t* prefix_end) const {
  std::string languages;  // Empty if we don't have a |navigation_controller|.
  Profile* profile = GetProfile();
  if (profile)
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

  GURL url(GetURL());
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  const base::string16 formatted_text =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(
                   url, languages, url_formatter::kFormatUrlOmitAll,
                   net::UnescapeRule::NORMAL, nullptr, prefix_end, nullptr),
          ChromeAutocompleteSchemeClassifier(profile));
  if (formatted_text.length() <= content::kMaxURLDisplayChars)
    return formatted_text;

  // Truncating the URL breaks editing and then pressing enter, but hopefully
  // people won't try to do much with such enormous URLs anyway. If this becomes
  // a real problem, we could perhaps try to keep some sort of different "elided
  // visible URL" where editing affects and reloads the "real underlying URL",
  // but this seems very tricky for little gain.
  return gfx::TruncateString(formatted_text, content::kMaxURLDisplayChars - 1,
                             gfx::CHARACTER_BREAK) +
         gfx::kEllipsisUTF16;
}

base::string16 ToolbarModelImpl::GetCorpusNameForMobile() const {
  if (!WouldPerformSearchTermReplacement(false))
    return base::string16();
  GURL url(GetURL());
  // If there is a query in the url fragment look for the corpus name there,
  // otherwise look for the corpus name in the query parameters.
  const std::string& query_str(google_util::HasGoogleSearchQueryParam(
      url.ref_piece()) ? url.ref() : url.query());
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

SecurityStateModel::SecurityLevel ToolbarModelImpl::GetSecurityLevel(
    bool ignore_editing) const {
  const content::WebContents* web_contents = delegate_->GetActiveWebContents();
  // If there is no active WebContents (which can happen during toolbar
  // initialization), assume no security style.
  if (!web_contents)
    return SecurityStateModel::NONE;
  const ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);

  // When editing, assume no security style.
  return (input_in_progress() && !ignore_editing)
             ? SecurityStateModel::NONE
             : model_client->GetSecurityInfo().security_level;
}

int ToolbarModelImpl::GetIcon() const {
  switch (GetSecurityLevel(false)) {
    case SecurityStateModel::NONE:
      return IDR_LOCATION_BAR_HTTP;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return IDR_OMNIBOX_HTTPS_VALID;
    case SecurityStateModel::SECURITY_WARNING:
      // Surface Dubious as Neutral.
      return IDR_LOCATION_BAR_HTTP;
    case SecurityStateModel::SECURITY_POLICY_WARNING:
      return IDR_OMNIBOX_HTTPS_POLICY_WARNING;
    case SecurityStateModel::SECURITY_ERROR:
      return IDR_OMNIBOX_HTTPS_INVALID;
  }

  NOTREACHED();
  return IDR_LOCATION_BAR_HTTP;
}

gfx::VectorIconId ToolbarModelImpl::GetVectorIcon() const {
#if !defined(OS_ANDROID) && !defined(OS_MACOSX) && !defined(OS_IOS)
  switch (GetSecurityLevel(false)) {
    case SecurityStateModel::NONE:
      return gfx::VectorIconId::LOCATION_BAR_HTTP;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID;
    case SecurityStateModel::SECURITY_WARNING:
      // Surface Dubious as Neutral.
      return gfx::VectorIconId::LOCATION_BAR_HTTP;
    case SecurityStateModel::SECURITY_POLICY_WARNING:
      return gfx::VectorIconId::BUSINESS;
    case SecurityStateModel::SECURITY_ERROR:
      return gfx::VectorIconId::LOCATION_BAR_HTTPS_INVALID;
  }
#endif

  NOTREACHED();
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

base::string16 ToolbarModelImpl::GetEVCertName() const {
  if (GetSecurityLevel(false) != SecurityStateModel::EV_SECURE)
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

  return !search::IsInstantNTP(delegate_->GetActiveWebContents());
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
  SecurityStateModel::SecurityLevel security_level =
      GetSecurityLevel(ignore_editing);
  return ((security_level == SecurityStateModel::NONE) ||
          (security_level == SecurityStateModel::SECURITY_ERROR))
             ? base::string16()
             : search_terms;
}
