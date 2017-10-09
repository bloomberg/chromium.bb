// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// A class, attached to WebContentses in browser windows, that is the
// JavaScriptDialogManager for them and handles displaying their dialogs.
//
// This is the primary mechanism for implementing auto-dismissing dialogs,
// dialogs that close when the user switches away to a different tab. Because
// JavaScript dialogs are synchronous and block arbitrary sets of renderers,
// they cannot be made tab-modal. Therefore the next best option is to make them
// auto-closing, so that they never block the user's access to other renderers.
//
// See http://bit.ly/project-oldspice for more details. Note that only part
// one of that design is implemented.
class JavaScriptDialogTabHelper
    : public content::JavaScriptDialogManager,
      public content::WebContentsObserver,
      public chrome::BrowserListObserver,
      public content::WebContentsUserData<JavaScriptDialogTabHelper> {
 public:
  explicit JavaScriptDialogTabHelper(content::WebContents* web_contents);
  ~JavaScriptDialogTabHelper() override;

  void SetDialogShownCallbackForTesting(base::OnceClosure callback);
  bool IsShowingDialogForTesting() const;

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           const GURL& alerting_frame_url,
                           content::JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

  // WebContentsObserver:
  void WasHidden() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::ReloadType reload_type) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

 private:
  friend class content::WebContentsUserData<JavaScriptDialogTabHelper>;
  enum class DismissalCause;

  void LogDialogDismissalCause(DismissalCause cause);

  void CloseDialog(DismissalCause cause,
                   bool success,
                   const base::string16& user_input);

  // The dialog being displayed on the observed WebContents.
  base::WeakPtr<JavaScriptDialog> dialog_;

  // The type of dialog being displayed. Only valid when |dialog_| is non-null.
  content::JavaScriptDialogType dialog_type_ =
      content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_ALERT;

  // The callback provided for when the dialog is closed. Usually the dialog
  // itself calls it, but in the cases where the dialog is closed not by the
  // user's input but by a call to |CloseDialog|, this class will call it.
  content::JavaScriptDialogManager::DialogClosedCallback dialog_callback_;

  base::OnceClosure dialog_shown_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogTabHelper);
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_H_
