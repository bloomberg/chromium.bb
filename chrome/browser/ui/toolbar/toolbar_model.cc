// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"
#include "chrome/common/chrome_constants.h"
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
#include "net/base/cert_status_flags.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;

ToolbarModel::ToolbarModel(ToolbarModelDelegate* delegate)
    : delegate_(delegate),
      input_in_progress_(false) {
}

ToolbarModel::~ToolbarModel() {
}

// ToolbarModel Implementation.
string16 ToolbarModel::GetText() const {
  GURL url(chrome::kAboutBlankURL);
  std::string languages;  // Empty if we don't have a |navigation_controller|.

  NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    Profile* profile =
        Profile::FromBrowserContext(navigation_controller->GetBrowserContext());
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
    NavigationEntry* entry = navigation_controller->GetVisibleEntry();
    if (!ShouldDisplayURL()) {
      url = GURL();
    } else if (entry) {
      url = entry->GetVirtualURL();
    }
  }
  if (url.spec().length() > content::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.GetOrigin() : GURL(url.scheme() + ":");
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, net::FormatUrl(url, languages, net::kFormatUrlOmitAll,
                          net::UnescapeRule::NORMAL, NULL, NULL, NULL));
}

bool ToolbarModel::ShouldDisplayURL() const {
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

  if (entry && entry->GetURL().SchemeIs(chrome::kExtensionScheme))
    return false;

  return true;
}

ToolbarModel::SecurityLevel ToolbarModel::GetSecurityLevel() const {
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

int ToolbarModel::GetIcon() const {
  static int icon_ids[NUM_SECURITY_LEVELS] = {
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_VALID,
    IDR_OMNIBOX_HTTPS_WARNING,
    IDR_OMNIBOX_HTTPS_INVALID,
  };
  DCHECK(arraysize(icon_ids) == NUM_SECURITY_LEVELS);
  return icon_ids[GetSecurityLevel()];
}

string16 ToolbarModel::GetEVCertName() const {
  DCHECK_EQ(GetSecurityLevel(), EV_SECURE);
  scoped_refptr<net::X509Certificate> cert;
  // Note: Navigation controller and active entry are guaranteed non-NULL or
  // the security level would be NONE.
  content::CertStore::GetInstance()->RetrieveCert(
      GetNavigationController()->GetVisibleEntry()->GetSSL().cert_id, &cert);
  return GetEVCertName(*cert);
}

// static
string16 ToolbarModel::GetEVCertName(const net::X509Certificate& cert) {
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

NavigationController* ToolbarModel::GetNavigationController() const {
  // This |current_tab| can be NULL during the initialization of the
  // toolbar during window creation (i.e. before any tabs have been added
  // to the window).
  WebContents* current_tab = delegate_->GetActiveWebContents();
  return current_tab ? &current_tab->GetController() : NULL;
}
