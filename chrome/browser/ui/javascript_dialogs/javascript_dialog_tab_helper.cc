// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_modal/javascript_dialog_manager.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(JavaScriptDialogTabHelper);

namespace {

const base::Feature kAutoDismissingDialogsFeature{
    "AutoDismissingDialogs", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsEnabled() {
  return base::FeatureList::IsEnabled(kAutoDismissingDialogsFeature);
}

content::JavaScriptDialogManager* AppModalDialogManager() {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

}  // namespace

JavaScriptDialogTabHelper::JavaScriptDialogTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

JavaScriptDialogTabHelper::~JavaScriptDialogTabHelper() {
}

void JavaScriptDialogTabHelper::WebContentsDestroyed() {
}

void JavaScriptDialogTabHelper::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    content::JavaScriptMessageType message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  SiteEngagementService* site_engagement_service = SiteEngagementService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  double engagement_score = site_engagement_service->GetScore(origin_url);
  switch (message_type) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT:
      UMA_HISTOGRAM_PERCENTAGE("JSDialogs.SiteEngagementOfDialogs.Alert",
                               engagement_score);
      break;
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM:
      UMA_HISTOGRAM_PERCENTAGE("JSDialogs.SiteEngagementOfDialogs.Confirm",
                               engagement_score);
      break;
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT:
      UMA_HISTOGRAM_PERCENTAGE("JSDialogs.SiteEngagementOfDialogs.Prompt",
                               engagement_score);
      break;
  }
  int32_t message_length = static_cast<int32_t>(message_text.length());
  if (engagement_score == 0) {
    UMA_HISTOGRAM_COUNTS("JSDialogs.CharacterCount.EngagementNone",
                         message_length);
  } else if (engagement_score < 1) {
    UMA_HISTOGRAM_COUNTS("JSDialogs.CharacterCount.EngagementLessThanOne",
                         message_length);
  } else if (engagement_score < 5) {
    UMA_HISTOGRAM_COUNTS("JSDialogs.CharacterCount.EngagementOneToFive",
                         message_length);
  } else {
    UMA_HISTOGRAM_COUNTS("JSDialogs.CharacterCount.EngagementHigher",
                         message_length);
  }

  if (IsEnabled()) {
    NOTREACHED() << "auto-dismissing dialog code does not yet exist";
  } else {
    AppModalDialogManager()->RunJavaScriptDialog(
        web_contents, origin_url, message_type, message_text,
        default_prompt_text, callback, did_suppress_message);
  }

  if (did_suppress_message) {
    UMA_HISTOGRAM_COUNTS("JSDialogs.CharacterCountUserSuppressed",
                         message_length);
  }
}

namespace {

void SaveUnloadUmaStats(
    double engagement_score,
    content::JavaScriptDialogManager::DialogClosedCallback callback,
    bool success,
    const base::string16& user_input) {
  if (success) {
    UMA_HISTOGRAM_PERCENTAGE("JSDialogs.SiteEngagementOfBeforeUnload.Leave",
                             engagement_score);
  } else {
    UMA_HISTOGRAM_PERCENTAGE("JSDialogs.SiteEngagementOfBeforeUnload.Stay",
                             engagement_score);
  }

  callback.Run(success, user_input);
}

}  // namespace

void JavaScriptDialogTabHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    bool is_reload,
    const DialogClosedCallback& callback) {
  // onbeforeunload dialogs are always handled with an app-modal dialog, because
  // - they are critical to the user not losing data
  // - they can be requested for tabs that are not foremost
  // - they can be requested for many tabs at the same time
  // and therefore auto-dismissal is inappropriate for them.

  SiteEngagementService* site_engagement_service = SiteEngagementService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  double engagement_score =
      site_engagement_service->GetScore(web_contents->GetLastCommittedURL());

  return AppModalDialogManager()->RunBeforeUnloadDialog(
      web_contents, is_reload,
      base::Bind(&SaveUnloadUmaStats, engagement_score, callback));
}

bool JavaScriptDialogTabHelper::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const base::string16* prompt_override) {
  if (!IsEnabled()) {
    return AppModalDialogManager()->HandleJavaScriptDialog(web_contents, accept,
                                                           prompt_override);
  }

  NOTREACHED() << "auto-dismissing dialog code does not yet exist";
  return false;
}

void JavaScriptDialogTabHelper::CancelActiveAndPendingDialogs(
    content::WebContents* web_contents) {
  // Cancel any app-modal dialogs that may be going.
  AppModalDialogManager()->CancelActiveAndPendingDialogs(web_contents);

  // More work here for the auto-dismissing dialogs.
}

void JavaScriptDialogTabHelper::ResetDialogState(
    content::WebContents* web_contents) {
  // Reset any app-modal dialog state that may exist.
  if (!IsEnabled())
    return AppModalDialogManager()->ResetDialogState(web_contents);

  // More work here for the auto-dismissing dialogs.
}
