// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/desktop_search_redirection_infobar_delegate.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/desktop_search_utils.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

namespace {

// Values for the Search.DesktopSearch.RedirectionInfobarCloseAction histogram.
enum DesktopSearchRedirectionInfobarCloseAction {
  DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_MANAGE_SEARCH_SETTINGS = 0,
  DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_DISMISS = 1,
  DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_IGNORE = 2,
  DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_MAX
};

void RecordDesktopSearchInfobarCloseActionHistogram(
    DesktopSearchRedirectionInfobarCloseAction action) {
  DCHECK_LT(action, DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "Search.DesktopSearch.RedirectionInfobarCloseAction", action,
      DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_MAX);
}

}  // namespace

void DesktopSearchRedirectionInfobarDelegate::Show(
    infobars::InfoBarManager* infobar_manager,
    const base::string16& default_search_engine_name,
    const base::Closure& manage_search_settings_callback,
    PrefService* pref_service) {
  DCHECK(infobar_manager);
  infobar_manager->AddInfoBar(infobar_manager->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new DesktopSearchRedirectionInfobarDelegate(
              default_search_engine_name, manage_search_settings_callback))));
  pref_service->SetBoolean(prefs::kDesktopSearchRedirectionInfobarShownPref,
                           true);
  base::RecordAction(
      base::UserMetricsAction("DesktopSearchRedirectionInfoBar_Shown"));
}

DesktopSearchRedirectionInfobarDelegate::
    DesktopSearchRedirectionInfobarDelegate(
        const base::string16& default_search_engine_name,
        const base::Closure& manage_search_settings_callback)
    : default_search_engine_name_(default_search_engine_name),
      manage_search_settings_callback_(manage_search_settings_callback),
      closed_by_user_(false) {}

DesktopSearchRedirectionInfobarDelegate::
    ~DesktopSearchRedirectionInfobarDelegate() {
  if (!closed_by_user_) {
    base::RecordAction(
        base::UserMetricsAction("DesktopSearchRedirectionInfoBar_Ignore"));
    RecordDesktopSearchInfobarCloseActionHistogram(
        DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_IGNORE);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
DesktopSearchRedirectionInfobarDelegate::GetIdentifier() const {
  return DESKTOP_SEARCH_REDIRECTION_INFOBAR_DELEGATE;
}

void DesktopSearchRedirectionInfobarDelegate::InfoBarDismissed() {
  base::RecordAction(
      base::UserMetricsAction("DesktopSearchRedirectionInfoBar_Dismiss"));
  RecordDesktopSearchInfobarCloseActionHistogram(
      DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_DISMISS);
  closed_by_user_ = true;
}

base::string16 DesktopSearchRedirectionInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_DESKTOP_SEARCH_REDIRECTION_INFOBAR_MESSAGE,
      default_search_engine_name_);
}

int DesktopSearchRedirectionInfobarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 DesktopSearchRedirectionInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_DESKTOP_SEARCH_REDIRECTION_INFOBAR_BUTTON);
}

bool DesktopSearchRedirectionInfobarDelegate::Accept() {
  base::RecordAction(base::UserMetricsAction(
      "DesktopSearchRedirectionInfoBar_ManageSearchSettings"));
  manage_search_settings_callback_.Run();

  // Close the infobar.
  RecordDesktopSearchInfobarCloseActionHistogram(
      DESKTOP_SEARCH_REDIRECTION_INFOBAR_CLOSE_ACTION_MANAGE_SEARCH_SETTINGS);
  closed_by_user_ = true;
  return true;
}
