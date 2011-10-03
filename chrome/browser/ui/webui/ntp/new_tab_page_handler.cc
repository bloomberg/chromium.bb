// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const int kIntroDisplayMax = 10;

// The URL of a knowledge-base article about the new NTP.
static const char kNTP4IntroURL[] =
  "http://www.google.com/support/chrome/bin/answer.py?answer=95451";

void NewTabPageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("closeNotificationPromo",
      base::Bind(&NewTabPageHandler::HandleCloseNotificationPromo,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("notificationPromoViewed",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoViewed,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("pageSelected",
      base::Bind(&NewTabPageHandler::HandlePageSelected,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("introMessageDismissed",
      base::Bind(&NewTabPageHandler::HandleIntroMessageDismissed,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("introMessageSeen",
      base::Bind(&NewTabPageHandler::HandleIntroMessageSeen,
                 base::Unretained(this)));
}

void NewTabPageHandler::HandleCloseNotificationPromo(const ListValue* args) {
  NotificationPromo notification_promo(
      Profile::FromWebUI(web_ui_)->GetPrefs(), NULL);
  notification_promo.HandleClosed();
  NotifyPromoResourceChanged();
}

void NewTabPageHandler::HandleNotificationPromoViewed(const ListValue* args) {
  NotificationPromo notification_promo(
      Profile::FromWebUI(web_ui_)->GetPrefs(), NULL);
  if (notification_promo.HandleViewed()) {
    NotifyPromoResourceChanged();
  }
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

void NewTabPageHandler::HandleIntroMessageDismissed(const ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, kIntroDisplayMax + 1);
}

void NewTabPageHandler::HandleIntroMessageSeen(const ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  int intro_displays = prefs->GetInteger(prefs::kNTP4IntroDisplayCount);
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, intro_displays + 1);
}

// static
void NewTabPageHandler::RegisterUserPrefs(PrefService* prefs) {
  // TODO(estade): should be syncable.
  prefs->RegisterIntegerPref(prefs::kNTPShownPage, APPS_PAGE_ID,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTP4IntroDisplayCount, 0,
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
    values->SetString("ntp4_intro_message",
                      l10n_util::GetStringUTF16(IDS_NTP4_INTRO_MESSAGE));
    values->SetString("ntp4_intro_url", kNTP4IntroURL);
    values->SetString("learn_more",
                      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  }
}

// static
void NewTabPageHandler::DismissIntroMessage(PrefService* prefs) {
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, kIntroDisplayMax + 1);
}

void NewTabPageHandler::NotifyPromoResourceChanged() {
  NotificationService* service = NotificationService::current();
  service->Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                  Source<NewTabPageHandler>(this),
                  NotificationService::NoDetails());
}
