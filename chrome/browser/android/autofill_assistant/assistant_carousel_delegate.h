// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_CAROUSEL_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_CAROUSEL_DELEGATE_H_

#include "base/android/scoped_java_ref.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for the carousel, to react on clicks on its chips.
class AssistantCarouselDelegate {
 public:
  explicit AssistantCarouselDelegate(UiControllerAndroid* ui_controller);
  ~AssistantCarouselDelegate();

  void OnChipSelected(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller,
                      jint index);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  UiControllerAndroid* ui_controller_;

  // Java-side AssistantCarouselDelegate object.
  base::android::ScopedJavaGlobalRef<jobject> java_assistant_carousel_delegate_;
};
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_CAROUSEL_DELEGATE_H_
