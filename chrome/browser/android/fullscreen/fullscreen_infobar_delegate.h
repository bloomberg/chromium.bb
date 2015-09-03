// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FULLSCREEN_FULLSCREEN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_FULLSCREEN_FULLSCREEN_INFOBAR_DELEGATE_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class InfoBarService;

// Native part of FullscreenInfoBarDelegate.java. This class is responsible
// for adding and removing the fullscreen infobar.
class FullscreenInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static bool RegisterFullscreenInfoBarDelegate(JNIEnv* env);

  FullscreenInfoBarDelegate(JNIEnv* env, jobject obj, GURL origin);
  ~FullscreenInfoBarDelegate() override;

  // Called to close the infobar.
  void CloseFullscreenInfoBar(JNIEnv* env, jobject obj);

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_delegate_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenInfoBarDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_FULLSCREEN_FULLSCREEN_INFOBAR_DELEGATE_H_
