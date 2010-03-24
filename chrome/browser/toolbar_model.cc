// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/toolbar_model.h"

#include "app/l10n_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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
  std::wstring languages;  // Empty if we don't have a |navigation_controller|.

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
  return net::FormatUrl(url, languages, true, UnescapeRule::NORMAL, NULL, NULL,
                        NULL);
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
      if (ssl.has_mixed_content())
        return SECURITY_WARNING;
      if ((ssl.cert_status() & net::CERT_STATUS_IS_EV) &&
          CertStore::GetSharedInstance()->RetrieveCert(ssl.cert_id(), NULL))
        return EV_SECURE;
      return SECURE;

    default:
      NOTREACHED();
      return NONE;
  }
}

int ToolbarModel::GetSecurityIcon() const {
  static int icon_ids[NUM_SECURITY_LEVELS] = {
    0,
    IDR_EV_SECURE,
    IDR_SECURE,
    IDR_SECURITY_WARNING,
    IDR_SECURITY_ERROR,
  };
  DCHECK(arraysize(icon_ids) == NUM_SECURITY_LEVELS);
  return icon_ids[GetSecurityLevel()];
}

void ToolbarModel::GetIconHoverText(std::wstring* text) const {
  DCHECK(text);
  text->clear();

  switch (GetSecurityLevel()) {
    case NONE:
      // There's no security icon, and thus no hover text.
      return;

    case EV_SECURE:
    case SECURE: {
      // Note: Navigation controller and active entry are guaranteed non-NULL or
      // the security level would be NONE.
      GURL url(GetNavigationController()->GetActiveEntry()->url());
      DCHECK(url.has_host());
      *text = l10n_util::GetStringF(IDS_SECURE_CONNECTION,
                                    UTF8ToWide(url.host()));
      return;
    }

    case SECURITY_WARNING:
      *text = SSLErrorInfo::CreateError(SSLErrorInfo::MIXED_CONTENTS, NULL,
                                        GURL()).short_description();
      return;

    case SECURITY_ERROR:
      // See note above.
      CreateErrorText(GetNavigationController()->GetActiveEntry(), text);
      // If the authentication is broken, we should always have at least one
      // error.
      DCHECK(!text->empty());
      return;

    default:
      NOTREACHED();
      return;
  }
}

std::wstring ToolbarModel::GetSecurityInfoText() const {
  switch (GetSecurityLevel()) {
    case NONE:
    case SECURE:
    case SECURITY_WARNING:
      return std::wstring();

    case EV_SECURE: {
      scoped_refptr<net::X509Certificate> cert;
      // See note in GetIconHoverText().
      CertStore::GetSharedInstance()->RetrieveCert(
          GetNavigationController()->GetActiveEntry()->ssl().cert_id(),
          &cert);
      return SSLManager::GetEVCertName(*cert);
    }

    case SECURITY_ERROR:
      return l10n_util::GetString(IDS_SECURITY_BROKEN);

    default:
      NOTREACHED();
      return std::wstring();
  }
}

NavigationController* ToolbarModel::GetNavigationController() const {
  // This |current_tab| can be NULL during the initialization of the
  // toolbar during window creation (i.e. before any tabs have been added
  // to the window).
  TabContents* current_tab = browser_->GetSelectedTabContents();
  return current_tab ? &current_tab->controller() : NULL;
}

void ToolbarModel::CreateErrorText(NavigationEntry* entry,
                                   std::wstring* text) const {
  const NavigationEntry::SSLStatus& ssl = entry->ssl();
  std::vector<SSLErrorInfo> errors;
  SSLErrorInfo::GetErrorsForCertStatus(ssl.cert_id(), ssl.cert_status(),
                                       entry->url(), &errors);
  if (ssl.has_mixed_content()) {
    errors.push_back(SSLErrorInfo::CreateError(SSLErrorInfo::MIXED_CONTENTS,
                                               NULL, GURL()));
  }
  if (ssl.has_unsafe_content()) {
    errors.push_back(SSLErrorInfo::CreateError(SSLErrorInfo::UNSAFE_CONTENTS,
                                               NULL, GURL()));
  }

  if (errors.empty()) {
    text->clear();
  } else if (errors.size() == 1) {
    *text = errors[0].short_description();
  } else {
    // Multiple errors.
    *text = l10n_util::GetString(IDS_SEVERAL_SSL_ERRORS);
    for (size_t i = 0; i < errors.size(); ++i) {
      text->append(L"\n");
      text->append(errors[i].short_description());
    }
  }
}
