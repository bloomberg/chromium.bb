// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_resource/notification_promo_helper.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"

namespace {

enum PromoAction {
  PROMO_VIEWED = 0,
  PROMO_CLOSED,
  PROMO_LINK_CLICKED,
  PROMO_ACTION_MAX,
};

}  // namespace

NewTabPageHandler::NewTabPageHandler() : page_switch_count_(0) {
}

NewTabPageHandler::~NewTabPageHandler() {}

void NewTabPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("notificationPromoClosed",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoClosed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("notificationPromoViewed",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoViewed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("notificationPromoLinkClicked",
      base::Bind(&NewTabPageHandler::HandleNotificationPromoLinkClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bubblePromoClosed",
      base::Bind(&NewTabPageHandler::HandleBubblePromoClosed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bubblePromoViewed",
      base::Bind(&NewTabPageHandler::HandleBubblePromoViewed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bubblePromoLinkClicked",
      base::Bind(&NewTabPageHandler::HandleBubblePromoLinkClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("pageSelected",
      base::Bind(&NewTabPageHandler::HandlePageSelected,
                 base::Unretained(this)));
}

void NewTabPageHandler::HandleNotificationPromoClosed(
    const base::ListValue* args) {
  web_resource::HandleNotificationPromoClosed(
      web_resource::NotificationPromo::NTP_NOTIFICATION_PROMO);
  Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleNotificationPromoViewed(
    const base::ListValue* args) {
  if (web_resource::HandleNotificationPromoViewed(
          web_resource::NotificationPromo::NTP_NOTIFICATION_PROMO)) {
    Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
  }
}

void NewTabPageHandler::HandleNotificationPromoLinkClicked(
    const base::ListValue* args) {
  DVLOG(1) << "HandleNotificationPromoLinkClicked";
}

void NewTabPageHandler::HandleBubblePromoClosed(const base::ListValue* args) {
  web_resource::HandleNotificationPromoClosed(
      web_resource::NotificationPromo::NTP_BUBBLE_PROMO);
  Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleBubblePromoViewed(const base::ListValue* args) {
  if (web_resource::HandleNotificationPromoViewed(
          web_resource::NotificationPromo::NTP_BUBBLE_PROMO))
    Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
}

void NewTabPageHandler::HandleBubblePromoLinkClicked(
    const base::ListValue* args) {
  DVLOG(1) << "HandleBubblePromoLinkClicked";
}

void NewTabPageHandler::HandlePageSelected(const base::ListValue* args) {
  page_switch_count_++;

  double page_id_double;
  CHECK(args->GetDouble(0, &page_id_double));
  int page_id = static_cast<int>(page_id_double);

  double index_double;
  CHECK(args->GetDouble(1, &index_double));
  int index = static_cast<int>(index_double);

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetInteger(prefs::kNtpShownPage, page_id | index);
}

// static
void NewTabPageHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(estade): should be syncable.
  registry->RegisterIntegerPref(prefs::kNtpShownPage, APPS_PAGE_ID);
}

// static
void NewTabPageHandler::GetLocalizedValues(Profile* profile,
                                           base::DictionaryValue* values) {
  values->SetInteger("apps_page_id", APPS_PAGE_ID);

  PrefService* prefs = profile->GetPrefs();
  int shown_page = prefs->GetInteger(prefs::kNtpShownPage);
  values->SetInteger("shown_page_type", shown_page & ~INDEX_MASK);
  values->SetInteger("shown_page_index", shown_page & INDEX_MASK);
}

void NewTabPageHandler::Notify(chrome::NotificationType notification_type) {
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(notification_type,
                  content::Source<NewTabPageHandler>(this),
                  content::NotificationService::NoDetails());
}
