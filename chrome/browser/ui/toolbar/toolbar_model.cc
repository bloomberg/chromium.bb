// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_util.h"

ToolbarModel::ToolbarModel(Browser* browser)
    : browser_(browser),
      input_in_progress_(false) {
}

ToolbarModel::~ToolbarModel() {
}

// ToolbarModel Implementation.
std::wstring ToolbarModel::GetText() const {
  GURL url(chrome::kAboutBlankURL);
  std::string languages;  // Empty if we don't have a |navigation_controller|.

  NavigationController* navigation_controller = GetNavigationController();
  if (navigation_controller) {
    languages = navigation_controller->profile()->GetPrefs()->GetString(
        prefs::kAcceptLanguages);
    NavigationEntry* entry = navigation_controller->GetActiveEntry();
    if (!navigation_controller->tab_contents()->ShouldDisplayURL()) {
      // Explicitly hide the URL for this tab.
      url = GURL();
    } else if (entry) {
      url = entry->virtual_url();
    }
  }
  if (url.spec().length() > chrome::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.GetOrigin() : GURL(url.scheme() + ":");
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return UTF16ToWideHack(
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          net::FormatUrl(url, languages, net::kFormatUrlOmitAll,
                         UnescapeRule::NORMAL, NULL, NULL, NULL)));
}

ToolbarModel::SecurityLevel ToolbarModel::GetSecurityLevel() const {
  if (input_in_progress_)  // When editing, assume no security style.
    return NONE;

  NavigationController* navigation_controller = GetNavigationController();
  if (!navigation_controller)  // We might not have a controller on init.
    return NONE;

  NavigationEntry* entry = navigation_controller->GetActiveEntry();
  if (!entry)
    return NONE;

  const NavigationEntry::SSLStatus& ssl = entry->ssl();
  switch (ssl.security_style()) {
    case SECURITY_STYLE_UNKNOWN:
    case SECURITY_STYLE_UNAUTHENTICATED:
      return NONE;

    case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return SECURITY_ERROR;

    case SECURITY_STYLE_AUTHENTICATED:
      if (ssl.displayed_insecure_content())
        return SECURITY_WARNING;
      if (net::IsCertStatusError(ssl.cert_status())) {
        DCHECK_EQ(ssl.cert_status() & net::CERT_STATUS_ALL_ERRORS,
                  net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
        return SECURITY_WARNING;
      }
      if ((ssl.cert_status() & net::CERT_STATUS_IS_EV) &&
          CertStore::GetInstance()->RetrieveCert(ssl.cert_id(), NULL))
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

std::wstring ToolbarModel::GetEVCertName() const {
  DCHECK_EQ(GetSecurityLevel(), EV_SECURE);
  scoped_refptr<net::X509Certificate> cert;
  // Note: Navigation controller and active entry are guaranteed non-NULL or
  // the security level would be NONE.
  CertStore::GetInstance()->RetrieveCert(
      GetNavigationController()->GetActiveEntry()->ssl().cert_id(), &cert);
  return UTF16ToWideHack(SSLManager::GetEVCertName(*cert));
}

NavigationController* ToolbarModel::GetNavigationController() const {
  // This |current_tab| can be NULL during the initialization of the
  // toolbar during window creation (i.e. before any tabs have been added
  // to the window).
  TabContents* current_tab = browser_->GetSelectedTabContents();
  return current_tab ? &current_tab->controller() : NULL;
}
