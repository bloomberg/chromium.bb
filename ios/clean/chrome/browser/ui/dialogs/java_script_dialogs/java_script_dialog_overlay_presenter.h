// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_WEB_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_WEB_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/browser_list/browser_user_data.h"
#include "ios/web/public/java_script_dialog_presenter.h"

class OverlayService;

// A concrete subclass of web::JavaScriptDialogPresenter that is associated with
// a Browser via its UserData.  It uses OverlayService to present dialogs.
class JavaScriptDialogOverlayPresenter
    : public BrowserUserData<JavaScriptDialogOverlayPresenter>,
      public web::JavaScriptDialogPresenter {
 public:
  ~JavaScriptDialogOverlayPresenter() override;

 private:
  friend class BrowserUserData<JavaScriptDialogOverlayPresenter>;

  // Private constructor used by factory method.
  explicit JavaScriptDialogOverlayPresenter(Browser* browser);

  // JavaScriptDialogPresenter:
  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           const web::DialogClosedCallback& callback) override;
  void CancelDialogs(web::WebState* web_state) override;

  // The OverlayService to use for the dialogs.
  OverlayService* overlay_service_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogOverlayPresenter);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_WEB_JAVA_SCRIPT_DIALOG_PRESENTER_H_
