// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"

#include "chrome/common/pref_names.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

static const int kIntroDisplayMax = 10;

void NewTabPageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("closePromo", NewCallback(
      this, &NewTabPageHandler::HandleClosePromo));
  web_ui_->RegisterMessageCallback("closeSyncNotification", NewCallback(
      this, &NewTabPageHandler::HandleCloseSyncNotification));
  web_ui_->RegisterMessageCallback("pageSelected", NewCallback(
      this, &NewTabPageHandler::HandlePageSelected));
  web_ui_->RegisterMessageCallback("navigationDotUsed", NewCallback(
      this, &NewTabPageHandler::HandleNavDotUsed));
  web_ui_->RegisterMessageCallback("introMessageSeen", NewCallback(
      this, &NewTabPageHandler::HandleIntroMessageSeen));
}

void NewTabPageHandler::HandleClosePromo(const ListValue* args) {
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(prefs::kNTPPromoClosed,
                                                      true);
  NotificationService* service = NotificationService::current();
  service->Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                  Source<NewTabPageHandler>(this),
                  NotificationService::NoDetails());
}

void NewTabPageHandler::HandleCloseSyncNotification(const ListValue* args) {
  ProfileSyncService* service =
      Profile::FromWebUI(web_ui_)->GetProfileSyncService();
  if (service)
    service->AcknowledgeSyncedTypes();
}

void NewTabPageHandler::HandlePageSelected(const ListValue* args) {
  double page_id_double;
  CHECK(args->GetDouble(0, &page_id_double));
  int page_id = static_cast<int>(page_id_double);

  double index_double;
  CHECK(args->GetDouble(1, &index_double));
  int index = static_cast<int>(index_double);

  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  prefs->SetInteger(prefs::kNTPShownPage, page_id | index);
}

void NewTabPageHandler::HandleNavDotUsed(const ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, kIntroDisplayMax + 1);
}

void NewTabPageHandler::HandleIntroMessageSeen(const ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  int intro_displays = prefs->GetInteger(prefs::kNTP4IntroDisplayCount);
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, intro_displays + 1);
}

// static
void NewTabPageHandler::HandleIntroMessageSuppressed(PrefService* prefs) {
  prefs->ClearPref(prefs::kNTP4SuppressIntroOnce);
}

// static
void NewTabPageHandler::RegisterUserPrefs(PrefService* prefs) {
  // TODO(estade): should be syncable.
  prefs->RegisterIntegerPref(prefs::kNTPShownPage, APPS_PAGE_ID,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTP4IntroDisplayCount, 0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kNTP4SuppressIntroOnce, false,
                             PrefService::UNSYNCABLE_PREF);
}

// static
void NewTabPageHandler::GetLocalizedValues(Profile* profile,
                                           DictionaryValue* values) {
  if (!NewTabUI::NTP4Enabled())
    return;

  values->SetInteger("most_visited_page_id", MOST_VISITED_PAGE_ID);
  values->SetInteger("apps_page_id", APPS_PAGE_ID);
  values->SetInteger("bookmarks_page_id", BOOKMARKS_PAGE_ID);

  PrefService* prefs = profile->GetPrefs();
  int shown_page = prefs->GetInteger(prefs::kNTPShownPage);
  values->SetInteger("shown_page_type", shown_page & ~INDEX_MASK);
  values->SetInteger("shown_page_index", shown_page & INDEX_MASK);

  int intro_displays = prefs->GetInteger(prefs::kNTP4IntroDisplayCount);
  if (intro_displays <= kIntroDisplayMax) {
    if (prefs->GetBoolean(prefs::kNTP4SuppressIntroOnce)) {
      // Clear pref so the bubble is only suppressed once.
      MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&NewTabPageHandler::HandleIntroMessageSuppressed, prefs));
    } else {
      values->SetString("ntp4_intro_message",
                        l10n_util::GetStringUTF16(IDS_NTP4_INTRO_MESSAGE));
    }
  }
}
