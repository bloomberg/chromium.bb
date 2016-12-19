// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_PRESENTER_IMPL_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_PRESENTER_IMPL_H_

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/web/public/java_script_dialog_presenter.h"

@class AlertCoordinator;
@class DialogPresenter;

class JavaScriptDialogPresenterImpl : public web::JavaScriptDialogPresenter {
 public:
  explicit JavaScriptDialogPresenterImpl(DialogPresenter* dialogPresenter);
  ~JavaScriptDialogPresenterImpl();

  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           const web::DialogClosedCallback& callback) override;

  void CancelDialogs(web::WebState* web_state) override;

 private:
  // The underlying DialogPresenter handling the dialog UI.
  base::scoped_nsobject<DialogPresenter> dialog_presenter_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogPresenterImpl);
};

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_PRESENTER_IMPL_H_
