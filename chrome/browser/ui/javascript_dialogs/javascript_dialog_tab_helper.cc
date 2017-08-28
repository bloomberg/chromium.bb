// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/text_elider.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(JavaScriptDialogTabHelper);

namespace {

app_modal::JavaScriptDialogManager* AppModalDialogManager() {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

bool IsWebContentsForemost(content::WebContents* web_contents) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

}  // namespace

// A note on the interaction between the JavaScriptDialogTabHelper and the
// JavaScriptDialog classes.
//
// Either side can start the process of closing a dialog, and we need to ensure
// that everything is properly torn down no matter which side initiates.
//
// If closing is initiated by the JavaScriptDialogTabHelper, then it will call
// CloseDialog(), which calls JavaScriptDialog::CloseDialogWithoutCallback().
// That will clear the callback inside of JavaScriptDialog, and start the
// JavaScriptDialog on its own path of destruction. CloseDialog() then calls
// ClearDialogInfo() which removes observers.
//
// If closing is initiated by the JavaScriptDialog, which is a self-deleting
// object, then it will make its callback. The callback will have been wrapped
// within JavaScriptDialogTabHelper::RunJavaScriptDialog() to be a call to
// JavaScriptDialogTabHelper::OnDialogClosed(), which, after doing the callback,
// again calls ClearDialogInfo() to remove observers.

enum class JavaScriptDialogTabHelper::DismissalCause {
  // This is used for a UMA histogram. Please never alter existing values, only
  // append new ones.
  TAB_HELPER_DESTROYED = 0,
  SUBSEQUENT_DIALOG_SHOWN = 1,
  HANDLE_DIALOG_CALLED = 2,
  CANCEL_DIALOGS_CALLED = 3,
  TAB_HIDDEN = 4,
  BROWSER_SWITCHED = 5,
  DIALOG_BUTTON_CLICKED = 6,
  TAB_NAVIGATED = 7,
  MAX,
};

JavaScriptDialogTabHelper::JavaScriptDialogTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

JavaScriptDialogTabHelper::~JavaScriptDialogTabHelper() {
  if (dialog_) {
    CloseDialog(false, base::string16(), DismissalCause::TAB_HELPER_DESTROYED);
  }
}

void JavaScriptDialogTabHelper::SetDialogShownCallbackForTesting(
    base::Closure callback) {
  dialog_shown_ = callback;
}

bool JavaScriptDialogTabHelper::IsShowingDialogForTesting() const {
  return !!dialog_;
}

void JavaScriptDialogTabHelper::RunJavaScriptDialog(
    content::WebContents* alerting_web_contents,
    const GURL& alerting_frame_url,
    content::JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  SiteEngagementService* site_engagement_service = SiteEngagementService::Get(
      Profile::FromBrowserContext(alerting_web_contents->GetBrowserContext()));
  double engagement_score =
      site_engagement_service->GetScore(alerting_frame_url);
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

  content::WebContents* parent_web_contents =
      WebContentsObserver::web_contents();
  bool foremost = IsWebContentsForemost(parent_web_contents);
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Alert", foremost);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Confirm", foremost);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Prompt", foremost);
      break;
  }

  if (!IsWebContentsForemost(parent_web_contents) &&
      dialog_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT) {
    // Don't allow "prompt" dialogs to steal the user's focus. TODO(avi):
    // Eventually, for subsequent phases of http://bit.ly/project-oldspice,
    // turn off focus stealing for other dialog types.
    *did_suppress_message = true;
    alerting_web_contents->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_WARNING,
        "A window.prompt() dialog generated by this page was suppressed "
        "because this page is not the active tab of the front window. "
        "Please make sure your dialogs are triggered by user interactions "
        "to avoid this situation. "
        "https://www.chromestatus.com/feature/5637107137642496");
    return;
  }

  if (dialog_) {
    // There's already a dialog up; clear it out.
    CloseDialog(false, base::string16(),
                DismissalCause::SUBSEQUENT_DIALOG_SHOWN);
  }

  // Enforce sane sizes. ElideRectangleString breaks horizontally, which isn't
  // strictly needed, but it restricts the vertical size, which is crucial.
  // This gives about 2000 characters, which is about the same as the
  // AppModalDialogManager provides, but allows no more than 24 lines.
  const int kMessageTextMaxRows = 24;
  const int kMessageTextMaxCols = 80;
  const size_t kDefaultPromptMaxSize = 2000;
  base::string16 truncated_message_text;
  gfx::ElideRectangleString(message_text, kMessageTextMaxRows,
                            kMessageTextMaxCols, false,
                            &truncated_message_text);
  base::string16 truncated_default_prompt_text;
  gfx::ElideString(default_prompt_text, kDefaultPromptMaxSize,
                   &truncated_default_prompt_text);

  base::string16 title = AppModalDialogManager()->GetTitle(
      alerting_web_contents, alerting_frame_url);
  dialog_callback_ = callback;
  dialog_type_ = dialog_type;
  dialog_ = JavaScriptDialog::Create(
      parent_web_contents, alerting_web_contents, title, dialog_type,
      truncated_message_text, truncated_default_prompt_text,
      base::Bind(&JavaScriptDialogTabHelper::OnDialogClosed,
                 base::Unretained(this), callback));

  BrowserList::AddObserver(this);

  // Message suppression is something that we don't give the user a checkbox
  // for any more. It was useful back in the day when dialogs were app-modal
  // and clicking the checkbox was the only way to escape a loop that the page
  // was doing, but now the user can just close the page.
  *did_suppress_message = false;

  if (!dialog_shown_.is_null()) {
    dialog_shown_.Run();
    dialog_shown_.Reset();
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
    CloseDialog(accept,
                prompt_override ? *prompt_override : dialog_->GetUserInput(),
                DismissalCause::HANDLE_DIALOG_CALLED);
    return true;
  }

  // Handle any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->HandleJavaScriptDialog(web_contents, accept,
                                                         prompt_override);
}

void JavaScriptDialogTabHelper::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  if (dialog_) {
    CloseDialog(false, base::string16(), DismissalCause::CANCEL_DIALOGS_CALLED);
  }

  // Cancel any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->CancelDialogs(web_contents, reset_state);
}

void JavaScriptDialogTabHelper::WasHidden() {
  if (dialog_)
    CloseDialog(false, base::string16(), DismissalCause::TAB_HIDDEN);
}

// This function handles the case where browser-side navigation (PlzNavigate) is
// enabled. DidStartNavigationToPendingEntry, below, handles the case where
// PlzNavigate is not enabled. TODO(avi): When the non-PlzNavigate code is
// removed, remove DidStartNavigationToPendingEntry.
void JavaScriptDialogTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Close the dialog if the user started a new navigation. This allows reloads
  // and history navigations to proceed.
  if (dialog_)
    CloseDialog(false, base::string16(), DismissalCause::TAB_NAVIGATED);
}

// This function handles the case where browser-side navigation (PlzNavigate) is
// not enabled. DidStartNavigation, above, handles the case where PlzNavigate is
// enabled. TODO(avi): When the non-PlzNavigate code is removed, remove
// DidStartNavigationToPendingEntry.
void JavaScriptDialogTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::ReloadType reload_type) {
  // Close the dialog if the user started a new navigation. This allows reloads
  // and history navigations to proceed.
  if (dialog_)
    CloseDialog(false, base::string16(), DismissalCause::TAB_NAVIGATED);
}

void JavaScriptDialogTabHelper::OnBrowserSetLastActive(Browser* browser) {
  if (dialog_ && !IsWebContentsForemost(web_contents())) {
    CloseDialog(false, base::string16(), DismissalCause::BROWSER_SWITCHED);
  }
}

void JavaScriptDialogTabHelper::LogDialogDismissalCause(
    JavaScriptDialogTabHelper::DismissalCause cause) {
  switch (dialog_type_) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Alert",
                                static_cast<int>(cause),
                                static_cast<int>(DismissalCause::MAX));
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Confirm",
                                static_cast<int>(cause),
                                static_cast<int>(DismissalCause::MAX));
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Prompt",
                                static_cast<int>(cause),
                                static_cast<int>(DismissalCause::MAX));
      break;
  }
}

void JavaScriptDialogTabHelper::OnDialogClosed(
    DialogClosedCallback callback,
    bool success,
    const base::string16& user_input) {
  LogDialogDismissalCause(DismissalCause::DIALOG_BUTTON_CLICKED);
  callback.Run(success, user_input);

  ClearDialogInfo();
}

void JavaScriptDialogTabHelper::CloseDialog(bool success,
                                            const base::string16& user_input,
                                            DismissalCause cause) {
  DCHECK(dialog_);
  LogDialogDismissalCause(cause);

  dialog_->CloseDialogWithoutCallback();
  dialog_callback_.Run(success, user_input);

  ClearDialogInfo();
}

void JavaScriptDialogTabHelper::ClearDialogInfo() {
  dialog_.reset();
  dialog_callback_.Reset();
  BrowserList::RemoveObserver(this);
}
