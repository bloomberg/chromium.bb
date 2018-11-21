// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_MANAGER_MANUAL_FILLING_VIEW_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_MANAGER_MANUAL_FILLING_VIEW_ANDROID_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"

namespace gfx {
class Image;
}

class ManualFillingController;

// This Android-specific implementation of the |ManualFillingViewInterface|
// is the native counterpart of the |PasswordAccessoryViewBridge| java class.
// It's owned by a ManualFillingController which is bound to an activity.
class ManualFillingViewAndroid : public ManualFillingViewInterface {
 public:
  // Builds the UI for the |controller|.
  explicit ManualFillingViewAndroid(ManualFillingController* controller);
  ~ManualFillingViewAndroid() override;

  // ManualFillingViewInterface:
  void OnItemsAvailable(const autofill::AccessorySheetData& data) override;
  void OnAutomaticGenerationStatusChanged(bool available) override;
  void CloseAccessorySheet() override;
  void SwapSheetWithKeyboard() override;
  void ShowWhenKeyboardIsVisible() override;
  void Hide() override;

  // Called from Java via JNI:
  void OnFaviconRequested(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint desiredSizeInPx,
      const base::android::JavaParamRef<jobject>& j_callback);
  void OnFillingTriggered(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean isPassword,
      const base::android::JavaParamRef<jstring>& textToFill);
  void OnOptionSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<_jstring*>& selectedOption);
  void OnGenerationRequested(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

 private:
  void OnImageFetched(
      const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
      const gfx::Image& image);

  // The controller provides data for this view and owns it.
  ManualFillingController* controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(ManualFillingViewAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_MANAGER_MANUAL_FILLING_VIEW_ANDROID_H_
