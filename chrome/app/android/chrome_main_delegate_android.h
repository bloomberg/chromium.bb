// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_
#define CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_

#include "chrome/app/chrome_main_delegate.h"
#include "content/public/browser/browser_main_runner.h"

// Android override of ChromeMainDelegate
class ChromeMainDelegateAndroid : public ChromeMainDelegate {
 public:
  static ChromeMainDelegateAndroid* Create();

  // Set up the JNI bindings.  Tie the Java methods with their native
  // counterparts.  Override to add more JNI bindings.
  virtual bool RegisterApplicationNativeMethods(JNIEnv* env);

 protected:
  ChromeMainDelegateAndroid();
  virtual ~ChromeMainDelegateAndroid();

  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;

  virtual void SandboxInitialized(const std::string& process_type) OVERRIDE;

  virtual int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) OVERRIDE;

 private:
  scoped_ptr<content::BrowserMainRunner> browser_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegateAndroid);
};

#endif  // CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_
