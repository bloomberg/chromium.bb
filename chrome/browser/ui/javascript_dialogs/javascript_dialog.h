// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

class AppModalDialogHelper;

class JavaScriptDialog {
 public:
  virtual ~JavaScriptDialog();

  static base::WeakPtr<JavaScriptDialog> Create(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback&
          dialog_callback);

  // Closes the dialog without sending a callback. This is useful when the
  // JavaScriptDialogTabHelper needs to make this dialog go away so that it can
  // respond to a call that requires it to make no callback or make a customized
  // one.
  virtual void CloseDialogWithoutCallback() = 0;

 protected:
  explicit JavaScriptDialog(content::WebContents* parent_web_contents);

 private:
  std::unique_ptr<AppModalDialogHelper> dialog_helper_;
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_H_
