// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const int kIntroDisplayMax = 10;

// The URL of a knowledge-base article about the new NTP.
static const char kNTP4IntroURL[] =
  "http://www.google.com/support/chrome/bin/answer.py?answer=95451";

WebUIMessageHandler* NewTabPageHandler::Attach(WebUI* web_ui) {
  // Record an open of the NTP with its default page type.
  PrefService* prefs = Profile::FromWebUI(web_ui)->GetPrefs();
  int shown_page_type = prefs->GetInteger(prefs::kNTPShownPage) >>
      kPageIdOffset;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.DefaultPageType",
                            shown_page_type, kHistogramEnumerationMax);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrial_Name);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("NewTabPage.DefaultPageType",
                                   kDefaultAppsTrial_Name),
        shown_page_type, kHistogramEnumerationMax);
  }

  return WebUIMessageHandler::Attach(web_ui);
}

void NewTabPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("closeNotificationPromo",
      base::Bind(&NewTabPageHandler::HandleCloseNotificationPromo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("notificationPromoViewed",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoViewed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("pageSelected",
      base::Bind(&NewTabPageHandler::HandlePageSelected,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("introMessageDismissed",
      base::Bind(&NewTabPageHandler::HandleIntroMessageDismissed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("introMessageSeen",
      base::Bind(&NewTabPageHandler::HandleIntroMessageSeen,
                 base::Unretained(this)));
}

void NewTabPageHandler::HandleCloseNotificationPromo(const ListValue* args) {
  scoped_refptr<NotificationPromo> notification_promo =
      NotificationPromo::Create(Profile::FromWebUI(web_ui()), NULL);
  notification_promo->HandleClosed();
  Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleNotificationPromoViewed(const ListValue* args) {
  scoped_refptr<NotificationPromo> notification_promo =
      NotificationPromo::Create(Profile::FromWebUI(web_ui_), NULL);
  if (notification_promo->HandleViewed())
    Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandlePageSelected(const ListValue* args) {
  double page_id_double;
  CHECK(args->GetDouble(0, &page_id_double));
  int page_id = static_cast<int>(page_id_double);

  double index_double;
  CHECK(args->GetDouble(1, &index_double));
  int index = static_cast<int>(index_double);

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetInteger(prefs::kNTPShownPage, page_id | index);

  int shown_page_type = page_id >> kPageIdOffset;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SelectedPageType",
                            shown_page_type, kHistogramEnumerationMax);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrial_Name);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("NewTabPage.SelectedPageType",
                                   kDefaultAppsTrial_Name),
        shown_page_type, kHistogramEnumerationMax);
  }
}

void NewTabPageHandler::HandleIntroMessageDismissed(const ListValue* args) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, kIntroDisplayMax + 1);
  Notify(chrome::NTP4_INTRO_PREF_CHANGED);
}

void NewTabPageHandler::HandleIntroMessageSeen(const ListValue* args) {
  PrefService* prefs = g_browser_process->local_state();
  int intro_displays = prefs->GetInteger(prefs::kNTP4IntroDisplayCount);
  prefs->SetInteger(prefs::kNTP4IntroDisplayCount, intro_displays + 1);
  Notify(chrome::NTP4_INTRO_PREF_CHANGED);
}

// static
void NewTabPageHandler::RegisterUserPrefs(PrefService* prefs) {
  // TODO(estade): should be syncable.
  prefs->RegisterIntegerPref(prefs::kNTPShownPage, APPS_PAGE_ID,
                             PrefService::UNSYNCABLE_PREF);
}

// static
void NewTabPageHandler::RegisterPrefs(PrefService* prefs) {
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

  PrefService* local_state = g_browser_process->local_state();
  int intro_displays = local_state->GetInteger(prefs::kNTP4IntroDisplayCount);
  // This preference used to exist in profile, so check the profile if it has
  // not been set in local state yet.
  if (!intro_displays) {
    prefs->RegisterIntegerPref(prefs::kNTP4IntroDisplayCount, 0,
                               PrefService::UNSYNCABLE_PREF);
    intro_displays = prefs->GetInteger(prefs::kNTP4IntroDisplayCount);
    if (intro_displays)
      local_state->SetInteger(prefs::kNTP4IntroDisplayCount, intro_displays);
  }
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
  // No need to send notification to update resource cache, because this method
  // is only called during startup before the ntp resource cache is constructed.
}

void NewTabPageHandler::Notify(chrome::NotificationType notification_type) {
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(notification_type,
                  content::Source<NewTabPageHandler>(this),
                  content::NotificationService::NoDetails());
}
