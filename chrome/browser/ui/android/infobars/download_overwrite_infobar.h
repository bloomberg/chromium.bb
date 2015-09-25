// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_DOWNLOAD_OVERWRITE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_DOWNLOAD_OVERWRITE_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"

namespace chrome {
namespace android {
class DownloadOverwriteInfoBarDelegate;
}
}

// A native-side implementation of an infobar to ask whether to overwrite
// an existing file with a new download.
class DownloadOverwriteInfoBar : public InfoBarAndroid {
 public:
  static scoped_ptr<infobars::InfoBar> CreateInfoBar(
      scoped_ptr<chrome::android::DownloadOverwriteInfoBarDelegate> delegate);
  ~DownloadOverwriteInfoBar() override;

 private:
  explicit DownloadOverwriteInfoBar(
      scoped_ptr<chrome::android::DownloadOverwriteInfoBarDelegate> delegate);

  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action) override;

  chrome::android::DownloadOverwriteInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(DownloadOverwriteInfoBar);
};

// Registers the native methods through JNI.
bool RegisterDownloadOverwriteInfoBarDelegate(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_DOWNLOAD_OVERWRITE_INFOBAR_H_
