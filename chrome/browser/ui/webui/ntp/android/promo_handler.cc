// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/promo_handler.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/browser/web_resource/notification_promo_mobile_ntp.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace {

// Promotion impression types for the NewTabPage.MobilePromo histogram.
// Should be kept in sync with the values in histograms.xml
enum PromoImpressionBuckets {
  PROMO_IMPRESSION_MOST_VISITED = 0,
  PROMO_IMPRESSION_OPEN_TABS = 1,
  PROMO_IMPRESSION_SYNC_PROMO = 2,
  PROMO_IMPRESSION_SEND_EMAIL_CLICKED = 3,
  PROMO_IMPRESSION_CLOSE_PROMO_CLICKED = 4,
  PROMO_IMPRESSION_BUCKET_BOUNDARY = 5
};

// Helper to record an impression in NewTabPage.MobilePromo histogram.
void RecordImpressionOnHistogram(PromoImpressionBuckets type) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MobilePromo", type,
                            PROMO_IMPRESSION_BUCKET_BOUNDARY);
}

// Helper to ask whether the promo is active.
bool CanShowNotificationPromo() {
  NotificationPromo notification_promo;
  notification_promo.InitFromPrefs(NotificationPromo::MOBILE_NTP_SYNC_PROMO);
  return notification_promo.CanShow();
}

// Helper to send out promo resource change notification.
void Notify(PromoHandler* ph, chrome::NotificationType notification_type) {
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(notification_type,
                  content::Source<PromoHandler>(ph),
                  content::NotificationService::NoDetails());
}

// Replaces all formatting markup in the promo with the corresponding HTML.
std::string ReplaceSimpleMarkupWithHtml(std::string text) {
  const std::string LINE_BREAK = "<br/>";
  const std::string SYNCGRAPHIC_IMAGE =
      "<div class=\"promo-sync-graphic\"></div>";
  const std::string BEGIN_HIGHLIGHT =
      "<div style=\"text-align: center\"><button class=\"promo-button\">";
  const std::string END_HIGHLIGHT = "</button></div>";
  const std::string BEGIN_LINK =
      "<span style=\"color: blue; text-decoration: underline;\">";
  const std::string END_LINK = "</span>";
  const std::string BEGIN_PROMO_AREA = "<div class=\"promo-action-target\">";
  const std::string END_PROMO_AREA = "</div>";

  ReplaceSubstringsAfterOffset(&text, 0, "LINE_BREAK", LINE_BREAK);
  ReplaceSubstringsAfterOffset(
      &text, 0, "SYNCGRAPHIC_IMAGE", SYNCGRAPHIC_IMAGE);
  ReplaceSubstringsAfterOffset(&text, 0, "BEGIN_HIGHLIGHT", BEGIN_HIGHLIGHT);
  ReplaceSubstringsAfterOffset(&text, 0, "END_HIGHLIGHT", END_HIGHLIGHT);
  ReplaceSubstringsAfterOffset(&text, 0, "BEGIN_LINK", BEGIN_LINK);
  ReplaceSubstringsAfterOffset(&text, 0, "END_LINK", END_LINK);
  return BEGIN_PROMO_AREA + text + END_PROMO_AREA;
}

}  // namespace

PromoHandler::PromoHandler() {
    // Watch for pref changes that cause us to need to re-inject promolines.
    registrar_.Add(this, chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                   content::NotificationService::AllSources());

    // Watch for sync service updates that could cause re-injections
    registrar_.Add(this, chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
                   content::NotificationService::AllSources());
}

PromoHandler::~PromoHandler() {
}

void PromoHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getPromotions",
      base::Bind(&PromoHandler::HandleGetPromotions,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("recordImpression",
      base::Bind(&PromoHandler::HandleRecordImpression,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("promoActionTriggered",
      base::Bind(&PromoHandler::HandlePromoActionTriggered,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("promoDisabled",
      base::Bind(&PromoHandler::HandlePromoDisabled,
                 base::Unretained(this)));
}

// static
void PromoHandler::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kNtpPromoDesktopSessionFound,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
}

void PromoHandler::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED == type ||
      chrome::NOTIFICATION_SYNC_CONFIGURE_DONE == type ||
      chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED == type) {
    // A change occurred to one of the preferences we care about
    CheckDesktopSessions();
    InjectPromoDecorations();
  } else {
    NOTREACHED() << "Unknown pref changed.";
  }
}

void PromoHandler::HandlePromoSendEmail(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  if (!profile)
    return;

  string16 data_subject, data_body, data_inv;
  if (!args || args->GetSize() < 3) {
    DVLOG(1) << "promoSendEmail: expected three args, got "
             << (args ? args->GetSize() : 0);
    return;
  }

  args->GetString(0, &data_subject);
  args->GetString(1, &data_body);
  args->GetString(2, &data_inv);
  if (data_inv.empty() || (data_subject.empty() && data_body.empty()))
    return;

  std::string data_email;
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (service && service->signin())
    data_email = service->signin()->GetAuthenticatedUsername();

  chrome::android::SendEmail(
      UTF8ToUTF16(data_email), data_subject, data_body, data_inv,
      EmptyString16());
  RecordImpressionOnHistogram(PROMO_IMPRESSION_SEND_EMAIL_CLICKED);
}

void PromoHandler::HandlePromoActionTriggered(const base::ListValue* /*args*/) {
  if (!CanShowNotificationPromo())
    return;

  NotificationPromoMobileNtp promo;
  if (!promo.InitFromPrefs())
    return;

  if (promo.action_type() == "ACTION_EMAIL")
    HandlePromoSendEmail(promo.action_args());
}

void PromoHandler::HandlePromoDisabled(const base::ListValue* /*args*/) {
  if (!CanShowNotificationPromo())
    return;

  NotificationPromo::HandleClosed(NotificationPromo::MOBILE_NTP_SYNC_PROMO);
  RecordImpressionOnHistogram(PROMO_IMPRESSION_CLOSE_PROMO_CLICKED);

  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                  content::Source<PromoHandler>(this),
                  content::NotificationService::NoDetails());
}

void PromoHandler::HandleGetPromotions(const base::ListValue* /*args*/) {
  CheckDesktopSessions();
  InjectPromoDecorations();
}

void PromoHandler::HandleRecordImpression(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(args && !args->empty());
  RecordPromotionImpression(UTF16ToASCII(ExtractStringValue(args)));
}

void PromoHandler::InjectPromoDecorations() {
  DictionaryValue result;
  if (FetchPromotion(&result))
    web_ui()->CallJavascriptFunction("ntp.setPromotions", result);
  else
    web_ui()->CallJavascriptFunction("ntp.clearPromotions");
}

void PromoHandler::RecordPromotionImpression(const std::string& id) {
  // Update number of views a promotion has received and trigger refresh
  // if it exceeded max_views set for the promotion.
  if (NotificationPromo::HandleViewed(
          NotificationPromo::MOBILE_NTP_SYNC_PROMO)) {
    Notify(this, chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
  }

  if (id == "most_visited")
    RecordImpressionOnHistogram(PROMO_IMPRESSION_MOST_VISITED);
  else if (id == "open_tabs")
    RecordImpressionOnHistogram(PROMO_IMPRESSION_OPEN_TABS);
  else if (id == "sync_promo")
    RecordImpressionOnHistogram(PROMO_IMPRESSION_SYNC_PROMO);
  else
    NOTREACHED() << "Unknown promotion impression: " << id;
}

bool PromoHandler::FetchPromotion(DictionaryValue* result) {
  DCHECK(result != NULL);
  if (!CanShowNotificationPromo())
    return false;

  NotificationPromoMobileNtp promo;
  if (!promo.InitFromPrefs())
    return false;

  DCHECK(!promo.text().empty());
  if (!DoesChromePromoMatchCurrentSync(
          promo.requires_sync(), promo.requires_mobile_only_sync())) {
    return false;
  }

  result->SetBoolean("promoIsAllowed", true);
  result->SetBoolean("promoIsAllowedOnMostVisited",
                     promo.show_on_most_visited());
  result->SetBoolean("promoIsAllowedOnOpenTabs", promo.show_on_open_tabs());
  result->SetBoolean("promoIsAllowedAsVC", promo.show_as_virtual_computer());
  result->SetString("promoVCTitle", promo.virtual_computer_title());
  result->SetString("promoVCLastSynced", promo.virtual_computer_lastsync());
  result->SetString("promoMessage", ReplaceSimpleMarkupWithHtml(promo.text()));
  result->SetString("promoMessageLong",
                    ReplaceSimpleMarkupWithHtml(promo.text_long()));
  return true;
}

bool PromoHandler::DoesChromePromoMatchCurrentSync(
    bool promo_requires_sync,
    bool promo_requires_no_active_desktop_sync_sessions) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return false;

  // If the promo doesn't require any sync, the requirements are fulfilled.
  if (!promo_requires_sync)
    return true;

  // The promo requires the sync; check that the sync service is active.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!service || !service->ShouldPushChanges())
    return false;

  // If the promo doesn't have specific requirements for the sync, it matches.
  if (!promo_requires_no_active_desktop_sync_sessions)
    return true;

  // If the promo requires mobile-only sync,
  // check that no desktop sessions are found.
  PrefService* prefs = profile->GetPrefs();
  return !prefs || !prefs->GetBoolean(prefs::kNtpPromoDesktopSessionFound);
}

void PromoHandler::CheckDesktopSessions() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  // Check if desktop sessions have already been found.
  PrefService* prefs = profile->GetPrefs();
  if (!prefs || prefs->GetBoolean(prefs::kNtpPromoDesktopSessionFound))
    return;

  // Check if the sync is currently active.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!service || !service->ShouldPushChanges())
    return;

  // Check if the sync has any open sessions.
  browser_sync::SessionModelAssociator* associator =
      service->GetSessionModelAssociator();
  if (!associator)
    return;

  // Let's see if there are no desktop sessions.
  std::vector<const browser_sync::SyncedSession*> sessions;
  ListValue session_list;
  if (!associator->GetAllForeignSessions(&sessions))
    return;

  for (size_t i = 0; i < sessions.size(); ++i) {
    const browser_sync::SyncedSession::DeviceType device_type =
        sessions[i]->device_type;
    if (device_type == browser_sync::SyncedSession::TYPE_WIN ||
        device_type == browser_sync::SyncedSession::TYPE_MACOSX ||
        device_type == browser_sync::SyncedSession::TYPE_LINUX) {
      // Found a desktop session: write out the pref.
      prefs->SetBoolean(prefs::kNtpPromoDesktopSessionFound, true);
      return;
    }
  }
}
