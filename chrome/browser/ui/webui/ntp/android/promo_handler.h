// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_PROMO_HANDLER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// The handler for JavaScript messages related to the Android NTP promo.
class PromoHandler : public content::WebUIMessageHandler,
                     public content::NotificationObserver {
 public:
  PromoHandler();
  virtual ~PromoHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Register preferences.
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // NotificationObserver override and implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Callback for the "promoSendEmail" message.
  // |args| is a list [ subject, body, app-chooser-message ].
  void HandlePromoSendEmail(const base::ListValue* args);

  // Callback for the "promoActionTriggered" message.
  // No arguments.
  void HandlePromoActionTriggered(const base::ListValue* args);

  // Callback for the "promoDisabled" message.
  // No arguments.
  void HandlePromoDisabled(const base::ListValue* args);

  // Callback for the "getPromotions" message.
  // No arguments.
  void HandleGetPromotions(const base::ListValue* args);

  // Callback for the "recordImpression" message.
  // |args| is a list with a name of a page to record an impression from.
  void HandleRecordImpression(const base::ListValue* args);

  // Gathers the promotion information and updates the page.
  void InjectPromoDecorations();

  // Records an impression; could trigger a refresh if max_views are exceeded.
  void RecordPromotionImpression(const std::string& id);

  // Fetches the active promotion and defines what should be passed to JS.
  // Returns true if the promotion should be shown and the |result| is ready.
  bool FetchPromotion(base::DictionaryValue* result);

  // Returns true if the Chrome Promo is allowed.
  bool DoesChromePromoMatchCurrentSync(
      bool promo_requires_sync,
      bool promo_requires_no_active_desktop_sync_sessions);

  // Updates the profile preference if any desktop session was discovered.
  void CheckDesktopSessions();

  // Registrar to receive notification on promo changes.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PromoHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_PROMO_HANDLER_H_
