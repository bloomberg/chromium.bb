// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

// User actions on the sync promo (aka "Sign in to Chrome").
enum SyncPromoUserFlowActionEnums {
  SYNC_PROMO_VIEWED,
  SYNC_PROMO_LEARN_MORE_CLICKED,
  SYNC_PROMO_ACCOUNT_HELP_CLICKED,
  SYNC_PROMO_CREATE_ACCOUNT_CLICKED,
  SYNC_PROMO_SKIP_CLICKED,
  SYNC_PROMO_SIGN_IN_ATTEMPTED,
  SYNC_PROMO_SIGNED_IN_SUCCESSFULLY,
  SYNC_PROMO_ADVANCED_CLICKED,
  SYNC_PROMO_ENCRYPTION_HELP_CLICKED,
  SYNC_PROMO_CANCELLED_AFTER_SIGN_IN,
  SYNC_PROMO_CONFIRMED_AFTER_SIGN_IN,
  SYNC_PROMO_CLOSED_TAB,
  SYNC_PROMO_CLOSED_WINDOW,
  SYNC_PROMO_LEFT_DURING_THROBBER,
  SYNC_PROMO_BUCKET_BOUNDARY,
  SYNC_PROMO_FIRST_VALID_JS_ACTION = SYNC_PROMO_LEARN_MORE_CLICKED,
  SYNC_PROMO_LAST_VALID_JS_ACTION = SYNC_PROMO_CONFIRMED_AFTER_SIGN_IN,
};

// This was added because of the need to change the existing UMA enum for the
// sync promo mid-flight. Ideally these values would be contiguous, but the
// real world is not always ideal.
static bool IsValidUserFlowAction(int action) {
  return (action >= SYNC_PROMO_FIRST_VALID_JS_ACTION &&
          action <= SYNC_PROMO_LAST_VALID_JS_ACTION) ||
         action == SYNC_PROMO_LEFT_DURING_THROBBER;
}

}  // namespace

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
  prefs->RegisterBooleanPref(prefs::kSyncPromoShowNTPBubble, false,
      PrefService::UNSYNCABLE_PREF);
}

void SyncPromoHandler::RegisterMessages() {
  // Keep a reference to the preferences service for convenience and it's
  // probably a little faster that getting it via Profile::FromWebUI() every
  // time we need to interact with preferences.
  prefs_ = Profile::FromWebUI(web_ui())->GetPrefs();
  DCHECK(prefs_);
  // Ignore events from view-source:chrome://syncpromo.
  if (!web_ui()->web_contents()->GetController().GetActiveEntry()->
          IsViewSourceMode()) {
    // Listen to see if the tab we're in gets closed.
    registrar_.Add(this, content::NOTIFICATION_TAB_CLOSING,
        content::Source<NavigationController>(
            &web_ui()->web_contents()->GetController()));
    // Listen to see if the window we're in gets closed.
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
        content::NotificationService::AllSources());
  }

  web_ui()->RegisterMessageCallback("SyncPromo:Close",
      base::Bind(&SyncPromoHandler::HandleCloseSyncPromo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncPromo:Initialize",
      base::Bind(&SyncPromoHandler::HandleInitializeSyncPromo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncPromo:RecordSignInAttempts",
      base::Bind(&SyncPromoHandler::HandleRecordSignInAttempts,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncPromo:RecordThrobberTime",
      base::Bind(&SyncPromoHandler::HandleRecordThrobberTime,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncPromo:ShowAdvancedSettings",
      base::Bind(&SyncPromoHandler::HandleShowAdvancedSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncPromo:UserFlowAction",
      base::Bind(&SyncPromoHandler::HandleUserFlowAction,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncPromo:UserSkipped",
      base::Bind(&SyncPromoHandler::HandleUserSkipped,
                 base::Unretained(this)));
  SyncSetupHandler::RegisterMessages();
}

void SyncPromoHandler::ShowGaiaSuccessAndClose() {
  RecordExperimentOutcomesOnSignIn();
  SyncSetupHandler::ShowGaiaSuccessAndClose();
}

void SyncPromoHandler::ShowGaiaSuccessAndSettingUp() {
  RecordExperimentOutcomesOnSignIn();
  SyncSetupHandler::ShowGaiaSuccessAndSettingUp();
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
        RecordUserFlowAction(SYNC_PROMO_CLOSED_TAB);
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSING: {
      // Make sure we're in the tab strip of the closing window.
      Browser* browser = content::Source<Browser>(source).ptr();
      if (browser->tabstrip_model()->GetWrapperIndex(
              web_ui()->web_contents()) != TabStripModel::kNoTab) {
        RecordUserFlowAction(SYNC_PROMO_CLOSED_WINDOW);
        window_already_closed_ = true;
      }
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

void SyncPromoHandler::StepWizardForShowSetupUI() {
}

void SyncPromoHandler::ShowSetupUI() {
  // SyncSetupWizard::Step should be called in StepWizardForShowSetupUI above,
  // but it causes the sync promo page to not set focus properly to the login
  // email address. This happens because focus is lost between the call to
  // StepWizardForShowSetupUI and ShowSetupUI.
  // TODO(binji): Move this function back and fix the focus the right way.
  ProfileSyncService* service =
      Profile::FromWebUI(web_ui())->GetProfileSyncService();
  service->get_wizard().Step(SyncSetupWizard::GetLoginState());
}

void SyncPromoHandler::HandleCloseSyncPromo(const base::ListValue* args) {
  CloseSyncSetup();

  // If the user has signed in then set the pref to show them NTP bubble
  // confirming that they're signed in.
  std::string username = prefs_->GetString(prefs::kGoogleServicesUsername);
  if (!username.empty())
    prefs_->SetBoolean(prefs::kSyncPromoShowNTPBubble, true);

  GURL url = SyncPromoUI::GetNextPageURLForSyncPromoURL(
      web_ui()->web_contents()->GetURL());
  OpenURLParams params(
      url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
  web_ui()->web_contents()->OpenURL(params);
}

void SyncPromoHandler::HandleInitializeSyncPromo(const base::ListValue* args) {
  // If the promo is also the Chrome launch page, we want to show the title and
  // log an event if we are running an experiment.
  bool is_launch_page = SyncPromoUI::GetIsLaunchPageForSyncPromoURL(
      web_ui()->web_contents()->GetURL());
  if (is_launch_page && sync_promo_trial::IsExperimentActive())
    sync_promo_trial::RecordUserSawMessage();
  base::FundamentalValue visible(is_launch_page);
  web_ui()->CallJavascriptFunction("SyncSetupOverlay.setPromoTitleVisible",
                                   visible);

  OpenSyncSetup();
  // We don't need to compute anything for this, just do this every time.
  RecordUserFlowAction(SYNC_PROMO_VIEWED);
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
  OpenURLParams params(
      GURL(url), Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
  web_ui()->web_contents()->OpenURL(params);
  RecordUserFlowAction(SYNC_PROMO_ADVANCED_CLICKED);
}

// TODO(dbeam): Replace with metricsHandler:recordHistogramTime when it exists.
void SyncPromoHandler::HandleRecordThrobberTime(const base::ListValue* args) {
  double time_double;
  CHECK(args->GetDouble(0, &time_double));
  UMA_HISTOGRAM_TIMES("SyncPromo.ThrobberTime",
                      base::TimeDelta::FromMilliseconds(time_double));
}

// TODO(dbeam): Replace with metricsHandler:recordHistogramCount when it exists.
void SyncPromoHandler::HandleRecordSignInAttempts(const base::ListValue* args) {
  double count_double;
  CHECK(args->GetDouble(0, &count_double));
  UMA_HISTOGRAM_COUNTS("SyncPromo.SignInAttempts", count_double);
}

void SyncPromoHandler::HandleUserFlowAction(const base::ListValue* args) {
  double action_double;
  CHECK(args->GetDouble(0, &action_double));
  int action = static_cast<int>(action_double);

  if (IsValidUserFlowAction(action))
    RecordUserFlowAction(action);
  else
    NOTREACHED() << "Attempt to record invalid user flow action on sync promo.";
}

void SyncPromoHandler::HandleUserSkipped(const base::ListValue* args) {
  SyncPromoUI::SetUserSkippedSyncPromo(Profile::FromWebUI(web_ui()));
  RecordUserFlowAction(SYNC_PROMO_SKIP_CLICKED);
}

int SyncPromoHandler::GetViewCount() const {
  // The locally persistent number of times the user has seen the sync promo.
  return prefs_->GetInteger(prefs::kSyncPromoViewCount);
}

int SyncPromoHandler::IncrementViewCountBy(size_t amount) {
  // Let the user increment by 0 if they really want.  It might be useful for a
  // weird way of sending preference change notifications...
  int adjusted = GetViewCount() + amount;
  prefs_->SetInteger(prefs::kSyncPromoViewCount, adjusted);
  return adjusted;
}

void SyncPromoHandler::RecordExperimentOutcomesOnSignIn() {
  if (sync_promo_trial::IsExperimentActive())
    sync_promo_trial::RecordUserSignedIn();
  if (sync_promo_trial::IsPartOfBrandTrialToEnable()) {
    bool is_start_up = SyncPromoUI::GetIsLaunchPageForSyncPromoURL(
        web_ui()->web_contents()->GetURL());
    Profile* profile = Profile::FromWebUI(web_ui());
    sync_promo_trial::RecordUserSignedInWithTrialBrand(is_start_up, profile);
  }
}

void SyncPromoHandler::RecordUserFlowAction(int action) {
  // Send an enumeration to our single user flow histogram.
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.UserFlow", action,
                            SYNC_PROMO_BUCKET_BOUNDARY);
}
