// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_ANDROID_H_

#include <jni.h>

// Returns whether the Java part has determined the child account status.
// In this case, |is_child_account| will be set to the value.
bool GetJavaChildAccountStatus(bool* is_child_account);

// Register native methods.
bool RegisterChildAccountService(JNIEnv* env);

void ChildStatusInvalidationReceived();

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_ANDROID_H_
