// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/sync_promo_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_details.h"

SyncPromoHandler::SyncPromoHandler(ProfileManager* profile_manager)
    : SyncSetupHandler(profile_manager),
      window_already_closed_(false) {
}

SyncPromoHandler::~SyncPromoHandler() {
}

// static
void SyncPromoHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kSyncPromoViewCount, 0,
      PrefService::UNSYNCABLE_PREF);
}

WebUIMessageHandler* SyncPromoHandler::Attach(WebUI* web_ui) {
  DCHECK(web_ui);
  // Keep a reference to the preferences service for convenience and it's
  // probably a little faster that getting it via Profile::FromWebUI() every
  // time we need to interact with preferences.
  prefs_ = Profile::FromWebUI(web_ui)->GetPrefs();
  DCHECK(prefs_);
  // Ignore events from view-source:chrome://syncpromo.
  if (!web_ui->tab_contents()->controller().GetActiveEntry()->
          IsViewSourceMode()) {
    // Listen to see if the tab we're in gets closed.
    registrar_.Add(this, content::NOTIFICATION_TAB_CLOSING,
        content::Source<NavigationController>(
            &web_ui->tab_contents()->controller()));
    // Listen to see if the window we're in gets closed.
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
        NotificationService::AllSources());
  }
  return SyncSetupHandler::Attach(web_ui);
}

void SyncPromoHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("SyncPromo:Close",
      base::Bind(&SyncPromoHandler::HandleCloseSyncPromo,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("SyncPromo:Initialize",
      base::Bind(&SyncPromoHandler::HandleInitializeSyncPromo,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("SyncPromo:UserFlowAction",
      base::Bind(&SyncPromoHandler::HandleUserFlowAction,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("SyncPromo:ShowAdvancedSettings",
      base::Bind(&SyncPromoHandler::HandleShowAdvancedSettings,
                 base::Unretained(this)));
  SyncSetupHandler::RegisterMessages();
}

void SyncPromoHandler::ShowConfigure(const base::DictionaryValue& args) {
  bool usePassphrase = false;
  args.GetBoolean("usePassphrase", &usePassphrase);

  if (usePassphrase) {
    // If a passphrase is required then we must show the configure pane.
    SyncSetupHandler::ShowConfigure(args);
  } else {
    // If no passphrase is required then skip the configure pane and sync
    // everything by default. This makes the first run experience simpler.
    // Note, there's an advanced link in the sync promo that takes users
    // to Settings where the configure pane is not skipped.
    SyncConfiguration configuration;
    configuration.sync_everything = true;
    DCHECK(flow());
    flow()->OnUserConfigured(configuration);
  }
}

void SyncPromoHandler::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_TAB_CLOSING: {
      if (!window_already_closed_)
        RecordUserFlowAction(extension_misc::SYNC_PROMO_CLOSED_TAB);
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSING: {
      // Make sure we're in the tab strip of the closing window.
      Browser* browser = content::Source<Browser>(source).ptr();
      if (browser->tabstrip_model()->GetWrapperIndex(
              web_ui_->tab_contents()) != TabStripModel::kNoTab) {
        RecordUserFlowAction(extension_misc::SYNC_PROMO_CLOSED_WINDOW);
        window_already_closed_ = true;
      }
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

void SyncPromoHandler::ShowSetupUI() {
  ProfileSyncService* service =
      Profile::FromWebUI(web_ui_)->GetProfileSyncService();
  service->get_wizard().Step(SyncSetupWizard::GetLoginState());
}

void SyncPromoHandler::HandleCloseSyncPromo(const base::ListValue* args) {
  CloseSyncSetup();

  // If the promo is being used as a welcome page then we want to close the
  // browser tab that the promo is in.
  if (IsWelcomePage()) {
    Browser* browser =
        BrowserList::FindBrowserWithTabContents(web_ui_->tab_contents());
    browser->CloseTabContents(web_ui_->tab_contents());
  } else {
    web_ui_->tab_contents()->OpenURL(GURL(chrome::kChromeUINewTabURL),
                                     GURL(), CURRENT_TAB,
                                     content::PAGE_TRANSITION_LINK);
  }
}

void SyncPromoHandler::HandleInitializeSyncPromo(const base::ListValue* args) {
  base::FundamentalValue visible(IsWelcomePage());
  web_ui_->CallJavascriptFunction("SyncSetupOverlay.setPromoTitleVisible",
                                  visible);

  OpenSyncSetup();
  // We don't need to compute anything for this, just do this every time.
  RecordUserFlowAction(extension_misc::SYNC_PROMO_VIEWED);
  // Increment view count first and show natural numbers in stats rather than 0
  // based starting point (if it happened to be our first time showing this).
  IncrementViewCountBy(1);
  // Record +1 for every view.  This is the only thing we record that's not part
  // of the user flow histogram.
  UMA_HISTOGRAM_COUNTS("SyncPromo.NumTimesViewed", GetViewCount());
}

void SyncPromoHandler::HandleShowAdvancedSettings(
    const base::ListValue* args) {
  CloseSyncSetup();
  std::string url(chrome::kChromeUISettingsURL);
  url += chrome::kSyncSetupSubPage;
  web_ui_->tab_contents()->OpenURL(GURL(url), GURL(), CURRENT_TAB,
                                   content::PAGE_TRANSITION_LINK);
}

void SyncPromoHandler::HandleUserFlowAction(const base::ListValue* args) {
  double action_double;
  CHECK(args->GetDouble(0, &action_double));
  int action = static_cast<int>(action_double);
  if (action >= extension_misc::SYNC_PROMO_FIRST_VALID_JS_ACTION &&
      action <= extension_misc::SYNC_PROMO_LAST_VALID_JS_ACTION) {
    RecordUserFlowAction(action);
  } else {
    NOTREACHED() << "Attempt to record invalid user flow action on sync promo.";
  }

  if (action == extension_misc::SYNC_PROMO_SKIP_CLICKED)
    SyncPromoUI::SetUserSkippedSyncPromo(Profile::FromWebUI(web_ui_));
}

int SyncPromoHandler::GetViewCount() const {
  // The locally persistent number of times the user has seen the sync promo.
  return prefs_->GetInteger(prefs::kSyncPromoViewCount);
}

int SyncPromoHandler::IncrementViewCountBy(unsigned int amount) {
  // Let the user increment by 0 if they really want.  It might be useful for a
  // weird way of sending preference change notifications...
  int adjusted = GetViewCount() + amount;
  prefs_->SetInteger(prefs::kSyncPromoViewCount, adjusted);
  return adjusted;
}

void SyncPromoHandler::RecordUserFlowAction(int action) {
  // Send an enumeration to our single user flow histogram.
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.UserFlow", action,
                            extension_misc::SYNC_PROMO_BUCKET_BOUNDARY);
}

bool SyncPromoHandler::IsWelcomePage() {
  // If there's no previous page on this tab then it means that the promo was
  // displayed at startup.
  return !web_ui_->tab_contents()->controller().CanGoBack();
}
