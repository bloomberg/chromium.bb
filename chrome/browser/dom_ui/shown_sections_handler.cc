// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/shown_sections_handler.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

namespace {

// Will cause an UMA notification if the mode of the new tab page
// was changed to hide/show the most visited thumbnails.
void NotifySectionDisabled(int new_mode, int old_mode, Profile *profile) {
  // If the oldmode HAD either thumbs or lists visible.
  bool old_had_it = (old_mode & THUMB) || (old_mode & LIST);
  bool new_has_it = (new_mode & THUMB) || (new_mode & LIST);

  if (old_had_it && !new_has_it) {
    UserMetrics::RecordAction(
        UserMetricsAction("ShowSections_RecentSitesDisabled"),
        profile);
  }

  if (new_has_it && !old_had_it) {
    UserMetrics::RecordAction(
        UserMetricsAction("ShowSections_RecentSitesEnabled"),
        profile);
  }
}

}  // namespace

ShownSectionsHandler::ShownSectionsHandler(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_service_->AddPrefObserver(prefs::kNTPShownSections, this);
}

ShownSectionsHandler::~ShownSectionsHandler() {
  pref_service_->RemovePrefObserver(prefs::kNTPShownSections, this);
}

void ShownSectionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getShownSections",
      NewCallback(this, &ShownSectionsHandler::HandleGetShownSections));
  dom_ui_->RegisterMessageCallback("setShownSections",
      NewCallback(this, &ShownSectionsHandler::HandleSetShownSections));
}

void ShownSectionsHandler::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(NotificationType::PREF_CHANGED == type);
  std::wstring* pref_name = Details<std::wstring>(details).ptr();
  DCHECK(*pref_name == prefs::kNTPShownSections);

  int sections = pref_service_->GetInteger(prefs::kNTPShownSections);
  FundamentalValue sections_value(sections);
  dom_ui_->CallJavascriptFunction(L"setShownSections", sections_value);
}

void ShownSectionsHandler::HandleGetShownSections(const Value* value) {
  int sections = pref_service_->GetInteger(prefs::kNTPShownSections);
  FundamentalValue sections_value(sections);
  dom_ui_->CallJavascriptFunction(L"onShownSections", sections_value);
}

void ShownSectionsHandler::HandleSetShownSections(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  const ListValue* list = static_cast<const ListValue*>(value);
  std::string mode_string;

  if (list->GetSize() < 1) {
    NOTREACHED() << "setShownSections called with too few arguments";
    return;
  }

  bool r = list->GetString(0, &mode_string);
  DCHECK(r) << "Missing value in setShownSections from the NTP Most Visited.";

  int mode = StringToInt(mode_string);
  int old_mode = pref_service_->GetInteger(prefs::kNTPShownSections);

  if (old_mode != mode) {
    NotifySectionDisabled(mode, old_mode, dom_ui_->GetProfile());
    pref_service_->SetInteger(prefs::kNTPShownSections, mode);
  }
}

// static
void ShownSectionsHandler::RegisterUserPrefs(PrefService* pref_service) {
  pref_service->RegisterIntegerPref(prefs::kNTPShownSections,
                                    THUMB | RECENT | TIPS | SYNC);
}

// static
void ShownSectionsHandler::MigrateUserPrefs(PrefService* pref_service,
                                            int old_pref_version,
                                            int new_pref_version) {
  bool changed = false;
  int shown_sections = pref_service->GetInteger(prefs::kNTPShownSections);

  if (old_pref_version < 1) {
    // TIPS was used in early builds of the NNTP but since it was removed before
    // Chrome 3.0 we want to ensure that it is shown by default.
    shown_sections |= TIPS | SYNC;
    changed = true;
  }

  if (old_pref_version < 2) {
    // LIST is no longer used. Change to THUMB.
    shown_sections &= ~LIST;
    shown_sections |= THUMB;
    changed = true;
  }

  if (changed)
    pref_service->SetInteger(prefs::kNTPShownSections, shown_sections);
}
