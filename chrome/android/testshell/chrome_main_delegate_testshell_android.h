// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_TESTSHELL_CHROME_MAIN_DELEGATE_TESTSHELL_ANDROID_H_
#define CHROME_ANDROID_TESTSHELL_CHROME_MAIN_DELEGATE_TESTSHELL_ANDROID_H_

#include "chrome/app/android/chrome_main_delegate_android.h"

class ChromeMainDelegateTestShellAndroid : public ChromeMainDelegateAndroid {
 public:
  ChromeMainDelegateTestShellAndroid();
  virtual ~ChromeMainDelegateTestShellAndroid();

  virtual bool RegisterApplicationNativeMethods(JNIEnv* env) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegateTestShellAndroid);
};

#endif  // CHROME_ANDROID_TESTSHELL_CHROME_MAIN_DELEGATE_TESTSHELL_ANDROID_H_
