// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"

#include "base/feature_list.h"
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
  if (!IsEnabled()) {
    return AppModalDialogManager()->RunJavaScriptDialog(
        web_contents, origin_url, message_type, message_text,
        default_prompt_text, callback, did_suppress_message);
  }

  NOTREACHED() << "auto-dismissing dialog code does not yet exist";
}

void JavaScriptDialogTabHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    bool is_reload,
    const DialogClosedCallback& callback) {
  // onbeforeunload dialogs are always handled with an app-modal dialog, because
  // - they are critical to the user not losing data
  // - they can be requested for tabs that are not foremost
  // - they can be requested for many tabs at the same time
  // and therefore auto-dismissal is inappropriate for them.

  return AppModalDialogManager()->RunBeforeUnloadDialog(web_contents, is_reload,
                                                        callback);
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
