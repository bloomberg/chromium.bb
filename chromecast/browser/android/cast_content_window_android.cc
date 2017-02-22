// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_content_window_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "jni/CastContentWindowAndroid_jni.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace chromecast {
namespace shell {

namespace {
base::android::ScopedJavaLocalRef<jobject> CreateJavaWindow(
    jlong nativeWindow) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastContentWindowAndroid_create(env, nativeWindow);
}
}  // namespace

// static
bool CastContentWindowAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate) {
  return base::WrapUnique(new CastContentWindowAndroid(delegate));
}

CastContentWindowAndroid::CastContentWindowAndroid(
    CastContentWindow::Delegate* delegate)
    : delegate_(delegate),
      java_window_(CreateJavaWindow(reinterpret_cast<jlong>(this))) {
  DCHECK(delegate_);
}

CastContentWindowAndroid::~CastContentWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_onNativeDestroyed(env, java_window_.obj());
}

void CastContentWindowAndroid::SetTransparent() {}

void CastContentWindowAndroid::ShowWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager) {
  DCHECK(window_manager);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents->GetJavaWebContents();

  Java_CastContentWindowAndroid_showWebContents(env, java_window_.obj(),
                                                java_web_contents.obj());
}

void CastContentWindowAndroid::OnActivityStopped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delegate_->OnWindowDestroyed();
}

void CastContentWindowAndroid::OnKeyDown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int keycode) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                         ui::KeyboardCodeFromAndroidKeyCode(keycode),
                         ui::EF_NONE);
  delegate_->OnKeyEvent(key_event);
}

}  // namespace shell
}  // namespace chromecast
