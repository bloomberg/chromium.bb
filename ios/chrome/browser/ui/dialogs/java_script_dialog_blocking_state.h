// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOGS_DIALOG_BLOCKING_JAVA_SCRIPT_DIALOG_BLOCKING_STATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOGS_DIALOG_BLOCKING_JAVA_SCRIPT_DIALOG_BLOCKING_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

// A helper object that tracks whether JavaScript dialogs should be blocked for
// a given WebState.  State is reset every time a navigation is committed.
class JavaScriptDialogBlockingState
    : public web::WebStateUserData<JavaScriptDialogBlockingState>,
      public web::WebStateObserver {
 public:
  ~JavaScriptDialogBlockingState() override;

  // Whether to show the blocking option for the next JavaScript dialog from its
  // WebState.
  bool show_blocking_option() { return dialog_count_ > 0; }
  // Whether the blocking option has been selected for its WebState's most
  // recent navigation.
  bool blocked() { return blocked_; }

  // Notifies the blocking state that a JavaScript dialog has been shown for its
  // WebState.
  void JavaScriptDialogWasShown() { ++dialog_count_; }
  // Notifies the blocking state that the JavaScript dialog blocking option has
  // been selected for its WebState's most recent navigation.
  void JavaScriptDialogBlockingOptionSelected() { blocked_ = true; }

 private:
  friend class web::WebStateUserData<JavaScriptDialogBlockingState>;

  // Private constructor.
  explicit JavaScriptDialogBlockingState(web::WebState* web_state);

  // WebStateObserver:
  void NavigationItemCommitted(
      web::WebState* web_state,
      const web::LoadCommittedDetails& load_details) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Whether to show the blocking option.
  size_t dialog_count_;
  // Whether the blocking option has been selected.
  bool blocked_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogBlockingState);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOGS_DIALOG_BLOCKING_JAVA_SCRIPT_DIALOG_BLOCKING_STATE_H_
