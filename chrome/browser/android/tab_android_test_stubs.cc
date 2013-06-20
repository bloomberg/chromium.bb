// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stubs for some Chrome for Android specific code that is
// needed to compile some tests.

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/auto_login_infobar_delegate.h"
#include "chrome/browser/ui/auto_login_infobar_delegate_android.h"

// static
TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  return NULL;
}

// static
TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, jobject obj) {
  return NULL;
}

// AutoLoginInfoBarDelegatAndroid empty implementation for test_shell.
// TODO(miguelg) remove once the AutoLoginInfoBar is upstreamed.
AutoLoginInfoBarDelegateAndroid::AutoLoginInfoBarDelegateAndroid(
    InfoBarService* owner,
    const AutoLoginInfoBarDelegate::Params& params)
    : AutoLoginInfoBarDelegate(owner, params) {}

AutoLoginInfoBarDelegateAndroid::~AutoLoginInfoBarDelegateAndroid() {}

bool AutoLoginInfoBarDelegateAndroid::Accept() {
  return false;
}

bool AutoLoginInfoBarDelegateAndroid::Cancel() {
  return false;
}

base::string16 AutoLoginInfoBarDelegateAndroid::GetMessageText() const {
  return base::string16();
}

// static
bool AutoLoginInfoBarDelegateAndroid::Register(JNIEnv* env) {
  return false;
}

