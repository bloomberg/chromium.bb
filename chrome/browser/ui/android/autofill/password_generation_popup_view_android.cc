// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/password_generation_popup_view_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/PasswordGenerationPopupBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/range/range.h"

namespace autofill {

PasswordGenerationPopupViewAndroid::PasswordGenerationPopupViewAndroid(
    PasswordGenerationPopupController* controller)
    : controller_(controller) {}

void PasswordGenerationPopupViewAndroid::SavedPasswordsLinkClicked(
    JNIEnv* env, jobject obj) {
  if (controller_)
    controller_->OnSavedPasswordsLinkClicked();
}

void PasswordGenerationPopupViewAndroid::Dismissed(JNIEnv* env, jobject obj) {
  if (controller_)
    controller_->ViewDestroyed();

  delete this;
}

void PasswordGenerationPopupViewAndroid::PasswordSelected(
    JNIEnv* env, jobject object) {
  if (controller_)
    controller_->PasswordAccepted();
}

// static
bool PasswordGenerationPopupViewAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

PasswordGenerationPopupViewAndroid::~PasswordGenerationPopupViewAndroid() {}

void PasswordGenerationPopupViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);

  java_object_.Reset(Java_PasswordGenerationPopupBridge_create(
      env, reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject().obj(),
      view_android->GetViewAndroidDelegate().obj()));

  UpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewAndroid::Hide() {
  controller_ = NULL;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordGenerationPopupBridge_hide(env, java_object_.obj());
}

gfx::Size PasswordGenerationPopupViewAndroid::GetPreferredSizeOfPasswordView() {
  static const int kUnusedSize = 0;
  return gfx::Size(kUnusedSize, kUnusedSize);
}

void PasswordGenerationPopupViewAndroid::UpdateBoundsAndRedrawPopup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordGenerationPopupBridge_setAnchorRect(
      env,
      java_object_.obj(),
      controller_->element_bounds().x(),
      controller_->element_bounds().y(),
      controller_->element_bounds().width(),
      controller_->element_bounds().height());

  ScopedJavaLocalRef<jstring> password =
      base::android::ConvertUTF16ToJavaString(env, controller_->password());
  ScopedJavaLocalRef<jstring> suggestion =
      base::android::ConvertUTF16ToJavaString(
          env, controller_->SuggestedText());
  ScopedJavaLocalRef<jstring> help =
      base::android::ConvertUTF16ToJavaString(env, controller_->HelpText());

  Java_PasswordGenerationPopupBridge_show(
      env,
      java_object_.obj(),
      controller_->IsRTL(),
      controller_->display_password(),
      password.obj(),
      suggestion.obj(),
      help.obj(),
      controller_->HelpTextLinkRange().start(),
      controller_->HelpTextLinkRange().end());
}

void PasswordGenerationPopupViewAndroid::PasswordSelectionUpdated() {}

bool PasswordGenerationPopupViewAndroid::IsPointInPasswordBounds(
        const gfx::Point& point) {
  NOTREACHED();
  return false;
}

// static
PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    PasswordGenerationPopupController* controller) {
  return new PasswordGenerationPopupViewAndroid(controller);
}

}  // namespace autofill
