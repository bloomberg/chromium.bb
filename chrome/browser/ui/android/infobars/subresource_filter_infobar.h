// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SUBRESOURCE_FILTER_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SUBRESOURCE_FILTER_INFOBAR_H_

#include "base/macros.h"
#include "chrome/browser/ui/android/content_settings/subresource_filter_infobar_delegate.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

class SubresourceFilterInfoBar : public ConfirmInfoBar {
 public:
  explicit SubresourceFilterInfoBar(
      std::unique_ptr<SubresourceFilterInfobarDelegate> delegate);

  ~SubresourceFilterInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SUBRESOURCE_FILTER_INFOBAR_H_
