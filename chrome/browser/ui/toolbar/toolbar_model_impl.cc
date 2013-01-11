// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
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

namespace {

// Returns true if |url| has the same host, port and path as the instant URL
// set via --instant-url.
bool IsForcedInstantURL(const GURL& url) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantURL)) {
    GURL instant_url(command_line->GetSwitchValueASCII(switches::kInstantURL));
    if (url.host() == instant_url.host() &&
        url.port() == instant_url.port() &&
        url.path() == instant_url.path()) {
      return true;
    }
  }
  return false;
}

// Coerces an instant URL to look like a regular search URL so we can extract
// query terms from the URL.
GURL ConvertInstantURLToSearchURL(const GURL& instant_url,
                                  const TemplateURL& template_url) {
  GURL search_url(template_url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(string16())));
  const std::string& scheme = search_url.scheme();
  const std::string& host = search_url.host();
  const std::string& port = search_url.port();
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  replacements.SetHostStr(host);
  replacements.SetPortStr(port);
  return instant_url.ReplaceComponents(replacements);
}

}  // namespace

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate)
    : delegate_(delegate),
      input_in_progress_(false) {
}

ToolbarModelImpl::~ToolbarModelImpl() {
}

// ToolbarModelImpl Implementation.
string16 ToolbarModelImpl::GetText(
    bool display_search_urls_as_search_terms) const {
  if (display_search_urls_as_search_terms) {
    string16 search_terms = TryToExtractSearchTermsFromURL();
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
  return !TryToExtractSearchTermsFromURL().empty();
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

  return true;
}

ToolbarModelImpl::SecurityLevel ToolbarModelImpl::GetSecurityLevel() const {
  if (input_in_progress_)  // When editing, assume no security style.
    return NONE;

  NavigationController* navigation_controller = GetNavigationController();
  if (!navigation_controller)  // We might not have a controller on init.
    return NONE;

  NavigationEntry* entry = navigation_controller->GetVisibleEntry();
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

string16 ToolbarModelImpl::TryToExtractSearchTermsFromURL() const {
  GURL url = GetURL();
  Profile* profile = GetProfile();

  // Ensure query extraction is enabled and query URL is HTTPS.
  if (!profile || !chrome::search::IsQueryExtractionEnabled(profile) ||
      !url.SchemeIs(chrome::kHttpsScheme))
    return string16();

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  TemplateURL* template_url = template_url_service->GetDefaultSearchProvider();
  if (!template_url)
    return string16();

  // Coerce URLs set via --instant-url to look like a regular search URL so we
  // can extract search terms from them.
  if (IsForcedInstantURL(url))
    url = ConvertInstantURLToSearchURL(url, *template_url);

  if (!template_url->HasSearchTermsReplacementKey(url))
    return string16();

  string16 result;
  template_url->ExtractSearchTermsFromURL(url, &result);
  return result;
}

Profile* ToolbarModelImpl::GetProfile() const {
  NavigationController* navigation_controller = GetNavigationController();
  return navigation_controller ?
      Profile::FromBrowserContext(navigation_controller->GetBrowserContext()) :
      NULL;
}
