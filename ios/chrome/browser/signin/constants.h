// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SIGNIN_CONSTANTS_H_

#include <Foundation/Foundation.h>

namespace ios {
class ChromeBrowserState;
}

// Error domain for authentication error.
extern NSString* kAuthenticationErrorDomain;

// The key in the user info dictionary containing the GoogleServiceAuthError
// code.
extern NSString* kGoogleServiceAuthErrorState;

typedef enum {
  // The error is wrapping a GoogleServiceAuthError.
  GOOGLE_SERVICE_AUTH_ERROR = -200,
  NO_AUTHENTICATED_USER = -201,
  CLIENT_ID_MISMATCH = -203,
  CLIENT_SECRET_MISMATCH = -204,
  AUTHENTICATION_FLOW_ERROR = -206,
  TIMED_OUT_FETCH_POLICY = -210,
} AuthenticationErrorCode;

// Enum is used to count the various sign in sources in the histogram
// |Signin.IOSSignInSource|.
typedef enum {
  // Unspecified sign-in source. This includes sign in operations started from
  // "Other Devices" and from the sync error infobar displayed for pre-SSO
  // signed in users.
  SIGN_IN_SOURCE_OTHER,
  // Sign in operation was started from first run.
  SIGN_IN_SOURCE_FIRST_RUN,
  // Sign in operation was started from the SSO recall promo.
  SIGN_IN_SOURCE_SSO_PROMO,
  // Sign in operation was started from settings.
  SIGN_IN_SOURCE_SETTINGS,
  // Sign in operation was started from as a pre-requisite of share operation.
  SIGN_IN_SOURCE_SHARE,
  // Sign in operation was started from as a pre-requisite of a print operation.
  SIGN_IN_SOURCE_PRINT,
  // Sign in operation was started from the re-sign in infobar.
  SIGN_IN_SOURCE_RESIGN_IN_INFOBAR,
  // Sign in operation was started from the Chrome to Device infobar.
  SIGN_IN_SOURCE_CHROME_TO_DEVICE_INFOBAR,
  // Sign in operation was started from the recent tabs panel.
  SIGN_IN_SOURCE_RECENT_TABS,
  // Sign in operation was started from the stars promo panel.
  SIGN_IN_SOURCE_STARS_PROMO,
  // NOTE: Add new sign in sources only immediately above this line. Also, make
  // sure the enum list for histogram |Signin.IOSSignInSource| in
  // tools/histogram/histograms.xml is updated with any change in here.
  SIGN_IN_SOURCE_COUNT
} SignInSource;

typedef enum {
  SHOULD_CLEAR_DATA_USER_CHOICE,
  SHOULD_CLEAR_DATA_CLEAR_DATA,
  SHOULD_CLEAR_DATA_MERGE_DATA,
} ShouldClearData;

// Enum is used to represent the action to be taken by the authentication once
// the user is successfully signed in.
typedef enum {
  POST_SIGNIN_ACTION_NONE,
  POST_SIGNIN_ACTION_START_SYNC,
} PostSignInAction;

namespace signin_ui {

// Completion callback for a sign-in operation.
// |signedIn| is true if the operation was successful and the user is now
// signed in.
// |signedInBrowserState| is the browser state that was signed in.
typedef void (^CompletionCallback)(
    BOOL signedIn,
    ios::ChromeBrowserState* signedInBrowserState);

}  // namespace signin_ui

#endif  // IOS_CHROME_BROWSER_SIGNIN_CONSTANTS_H_
