// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_delegate.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(JavaScriptDialogTabHelper);

namespace {

const base::Feature kAutoDismissingDialogsFeature{
    "AutoDismissingDialogs", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsEnabled() {
  return base::FeatureList::IsEnabled(kAutoDismissingDialogsFeature);
}

app_modal::JavaScriptDialogManager* AppModalDialogManager() {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

bool IsWebContentsForemost(content::WebContents* web_contents) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

}  // namespace

JavaScriptDialogTabHelper::JavaScriptDialogTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

JavaScriptDialogTabHelper::~JavaScriptDialogTabHelper() {
}

void JavaScriptDialogTabHelper::RunJavaScriptDialog(
    content::WebContents* alerting_web_contents,
    const GURL& origin_url,
    content::JavaScriptMessageType message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  SiteEngagementService* site_engagement_service = SiteEngagementService::Get(
      Profile::FromBrowserContext(alerting_web_contents->GetBrowserContext()));
  double engagement_score = site_engagement_service->GetScore(origin_url);
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
    content::WebContents* parent_web_contents =
        WebContentsObserver::web_contents();

    if (!IsWebContentsForemost(parent_web_contents) &&
        message_type == content::JAVASCRIPT_MESSAGE_TYPE_PROMPT) {
      // Don't allow "prompt" dialogs to steal the user's focus. TODO(avi):
      // Eventually, for subsequent phases of http://bit.ly/project-oldspice,
      // turn off focus stealing for other dialog types.
      *did_suppress_message = true;
      callback.Run(false, base::string16());
      return;
    }

    if (dialog_) {
      // There's already a dialog up; clear it out.
      CloseDialog(false, false, base::string16());
    }

    parent_web_contents->GetDelegate()->ActivateContents(parent_web_contents);

    base::string16 title =
        AppModalDialogManager()->GetTitle(alerting_web_contents, origin_url);
    dialog_callback_ = callback;
    BrowserList::AddObserver(this);
    dialog_ = JavaScriptDialog::Create(
        parent_web_contents, alerting_web_contents, title, message_type,
        message_text, default_prompt_text, callback);

    // Message suppression is something that we don't give the user a checkbox
    // for any more. It was useful back in the day when dialogs were app-modal
    // and clicking the checkbox was the only way to escape a loop that the page
    // was doing, but now the user can just close the page.
    *did_suppress_message = false;
  } else {
    AppModalDialogManager()->RunJavaScriptDialog(
        alerting_web_contents, origin_url, message_type, message_text,
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
  if (dialog_) {
    CloseDialog(false /*suppress_callback*/, accept,
                prompt_override ? *prompt_override : base::string16());
    return true;
  }

  // Handle any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->HandleJavaScriptDialog(web_contents, accept,
                                                         prompt_override);
}

void JavaScriptDialogTabHelper::CancelDialogs(
    content::WebContents* web_contents,
    bool suppress_callbacks,
    bool reset_state) {
  if (dialog_)
    CloseDialog(suppress_callbacks, false, base::string16());

  // Cancel any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->CancelDialogs(
      web_contents, suppress_callbacks, reset_state);
}

void JavaScriptDialogTabHelper::WasHidden() {
  if (dialog_)
    CloseDialog(false, false, base::string16());
}

void JavaScriptDialogTabHelper::OnBrowserSetLastActive(Browser* browser) {
  if (dialog_ && !IsWebContentsForemost(web_contents()))
    CloseDialog(false, false, base::string16());
}

void JavaScriptDialogTabHelper::CloseDialog(bool suppress_callback,
                                            bool success,
                                            const base::string16& user_input) {
  DCHECK(dialog_);

  dialog_->CloseDialogWithoutCallback();
  if (!suppress_callback)
    dialog_callback_.Run(success, user_input);

  dialog_.reset();
  dialog_callback_.Reset();
  BrowserList::RemoveObserver(this);
}
