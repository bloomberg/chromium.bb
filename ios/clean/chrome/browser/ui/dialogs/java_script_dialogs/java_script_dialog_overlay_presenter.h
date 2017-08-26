// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_WEB_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_WEB_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/java_script_dialog_presenter.h"
#import "ios/web/public/web_state/web_state_user_data.h"

class OverlayService;

// A concrete subclass of web::JavaScriptDialogPresenter that is associated with
// a WebState via its UserData.  It uses OverlayService to present dialogs.
class JavaScriptDialogOverlayPresenter
    : public web::JavaScriptDialogPresenter,
      public web::WebStateUserData<JavaScriptDialogOverlayPresenter> {
 public:
  // Factory method for a presenter that uses |overlay_service| to show dialogs.
  static void CreateForWebState(web::WebState* web_state,
                                OverlayService* overlay_service);

  ~JavaScriptDialogOverlayPresenter() override;

 private:
  friend class web::WebStateUserData<JavaScriptDialogOverlayPresenter>;

  // Private constructor.  New instances should be created via
  // CreateForWebState() and accessed via FromWebState().
  explicit JavaScriptDialogOverlayPresenter(web::WebState* web_state,
                                            OverlayService* overlay_service);

  // JavaScriptDialogPresenter:
  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           const web::DialogClosedCallback& callback) override;
  void CancelDialogs(web::WebState* web_state) override;

  // The WebState with which this presenter is associated.
  web::WebState* web_state_;
  // The OverlayService to use for the dialogs.
  OverlayService* overlay_service_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogOverlayPresenter);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_WEB_JAVA_SCRIPT_DIALOG_PRESENTER_H_
