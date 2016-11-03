// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_JAVASCRIPT_DIALOG_BLOCKING_UTIL_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_JAVASCRIPT_DIALOG_BLOCKING_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

// Returns true if the dialog blocking option should be shown in JavaScript
// dialogs being shown for |web_state|.  The blocking option will be
// shown when multiple JavaScript dialogs have been displayed for
// |web_state|.
bool ShouldShowDialogBlockingOption(web::WebState* web_state);

// Called when a JavaScript dialog is shown for |web_state|.
void JavaScriptDialogWasShown(web::WebState* web_state);

// Returns true if the blocking option was selected for a previous JavaScript
// dialog shown for |web_state|.
bool ShouldBlockJavaScriptDialogs(web::WebState* web_state);

// Called when the user has selected the dialog blocking option on a
// JavaScript dialog shown for |web_state|.
void DialogBlockingOptionSelected(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_JAVASCRIPT_DIALOG_BLOCKING_UTIL_H_
