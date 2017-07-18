// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class InstantAppsInfoBarDelegate : public ConfirmInfoBarDelegate,
                                   public content::WebContentsObserver {
 public:
  ~InstantAppsInfoBarDelegate() override;

  static void Create(content::WebContents* web_contents,
                     const jobject jdata,
                     const std::string& url);

  base::android::ScopedJavaGlobalRef<jobject> data() { return data_; }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit InstantAppsInfoBarDelegate(content::WebContents* web_contents,
                                      const jobject jdata,
                                      const std::string& url);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  void InfoBarDismissed() override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> data_;
  std::string url_;

  DISALLOW_COPY_AND_ASSIGN(InstantAppsInfoBarDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_
