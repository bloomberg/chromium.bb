// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class JavaScriptDialogTabHelper
    : public content::WebContentsObserver,
      public content::JavaScriptDialogManager,
      public content::WebContentsUserData<JavaScriptDialogTabHelper> {
 public:
  explicit JavaScriptDialogTabHelper(content::WebContents* web_contents);
  ~JavaScriptDialogTabHelper() override;

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           const GURL& origin_url,
                           content::JavaScriptMessageType message_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           const DialogClosedCallback& callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             bool is_reload,
                             const DialogClosedCallback& callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;
  void CancelActiveAndPendingDialogs(
      content::WebContents* web_contents) override;
  void ResetDialogState(content::WebContents* web_contents) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<JavaScriptDialogTabHelper>;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogTabHelper);
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_H_
