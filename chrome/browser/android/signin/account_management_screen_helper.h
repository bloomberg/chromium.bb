// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_MANAGEMENT_SCREEN_HELPER_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_MANAGEMENT_SCREEN_HELPER_H_

#include <jni.h>

#include "base/basictypes.h"
#include "chrome/browser/signin/signin_header_helper.h"

class Profile;

// The glue for Java-side implementation of AccountManagementScreenHelper.
class AccountManagementScreenHelper {
 public:
  // Registers AccountManagementScreenHelper native methods through JNI.
  static bool Register(JNIEnv* env);

  // Opens the account management screen.
  static void OpenAccountManagementScreen(Profile* profile,
                                          signin::GAIAServiceType service_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagementScreenHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_MANAGEMENT_SCREEN_HELPER_H_
