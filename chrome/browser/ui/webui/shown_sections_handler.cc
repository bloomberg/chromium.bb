// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/shown_sections_handler.h"

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

namespace {

// Will cause an UMA notification if the mode of the new tab page
// was changed to hide/show the most visited thumbnails.
// TODO(aa): Needs to be updated to match newest NTP - http://crbug.com/57440
void NotifySectionDisabled(int new_mode, int old_mode, Profile *profile) {
  // If the oldmode HAD either thumbs or lists visible.
  bool old_had_it = (old_mode & THUMB) && !(old_mode & MENU_THUMB);
  bool new_has_it = (new_mode & THUMB) && !(new_mode & MENU_THUMB);

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

// static
int ShownSectionsHandler::GetShownSections(PrefService* prefs) {
  return prefs->GetInteger(prefs::kNTPShownSections);
}

// static
void ShownSectionsHandler::SetShownSection(PrefService* prefs,
                                           Section section) {
  int shown_sections = GetShownSections(prefs);
  shown_sections &= ~ALL_SECTIONS_MASK;
  shown_sections |= section;
  prefs->SetInteger(prefs::kNTPShownSections, shown_sections);
}

ShownSectionsHandler::ShownSectionsHandler(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(prefs::kNTPShownSections, this);
}

void ShownSectionsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getShownSections",
      NewCallback(this, &ShownSectionsHandler::HandleGetShownSections));
  web_ui_->RegisterMessageCallback("setShownSections",
      NewCallback(this, &ShownSectionsHandler::HandleSetShownSections));
}

void ShownSectionsHandler::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    DCHECK(*pref_name == prefs::kNTPShownSections);
    int sections = pref_service_->GetInteger(prefs::kNTPShownSections);
    FundamentalValue sections_value(sections);
    web_ui_->CallJavascriptFunction(L"setShownSections", sections_value);
  } else {
    NOTREACHED();
  }
}

void ShownSectionsHandler::HandleGetShownSections(const ListValue* args) {
  int sections = GetShownSections(pref_service_);
  FundamentalValue sections_value(sections);
  web_ui_->CallJavascriptFunction(L"onShownSections", sections_value);
}

void ShownSectionsHandler::HandleSetShownSections(const ListValue* args) {
  double mode_double;
  CHECK(args->GetDouble(0, &mode_double));
  int mode = static_cast<int>(mode_double);
  int old_mode = pref_service_->GetInteger(prefs::kNTPShownSections);

  if (old_mode != mode) {
    NotifySectionDisabled(mode, old_mode, web_ui_->GetProfile());
    pref_service_->SetInteger(prefs::kNTPShownSections, mode);
  }
}

// static
void ShownSectionsHandler::RegisterUserPrefs(PrefService* pref_service) {
#if defined(OS_CHROMEOS)
  // Default to have expanded APPS and all other sections are minimized.
  pref_service->RegisterIntegerPref(prefs::kNTPShownSections,
                                    APPS | MENU_THUMB | MENU_RECENT);
#else
  pref_service->RegisterIntegerPref(prefs::kNTPShownSections, THUMB);
#endif
}

// static
void ShownSectionsHandler::MigrateUserPrefs(PrefService* pref_service,
                                            int old_pref_version,
                                            int new_pref_version) {
  // Nothing to migrate for default kNTPShownSections value.
  const PrefService::Preference* shown_sections_pref =
      pref_service->FindPreference(prefs::kNTPShownSections);
  if (shown_sections_pref->IsDefaultValue())
    return;

  bool changed = false;
  int shown_sections = pref_service->GetInteger(prefs::kNTPShownSections);

  if (old_pref_version < 3) {
    // In version 3, we went from being able to show multiple sections to being
    // able to show only one expanded at a time. The only two expandable
    // sections are APPS and THUMBS.
    if (shown_sections & APPS)
      shown_sections = APPS;
    else
      shown_sections = THUMB;

    changed = true;
  }

  if (changed)
    pref_service->SetInteger(prefs::kNTPShownSections, shown_sections);
}

// static
void ShownSectionsHandler::OnExtensionInstalled(PrefService* prefs,
                                                const Extension* extension) {
  if (extension->is_app()) {
    int mode = prefs->GetInteger(prefs::kNTPShownSections);

    // De-menu-mode the apps section.
    mode &= ~MENU_APPS;

    // Hide any open sections.
    mode &= ~ALL_SECTIONS_MASK;

    // Show the apps section.
    mode |= APPS;

    prefs->SetInteger(prefs::kNTPShownSections, mode);
  }
}
