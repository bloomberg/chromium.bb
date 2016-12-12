// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/joystick_scroll_provider.h"

#include "base/supports_user_data.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "jni/JoystickScrollProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

const void* const kJoystickScrollUserDataKey = &kJoystickScrollUserDataKey;

namespace {
const double MILLISECONDS_IN_SECOND = 1000.0;
}

// A helper class to attach JoystickScrollProvider to the WebContents.
class JoystickScrollProvider::UserData : public base::SupportsUserData::Data {
 public:
  explicit UserData(JoystickScrollProvider* rep) : rep_(rep) {}

 private:
  std::unique_ptr<JoystickScrollProvider> rep_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UserData);
};

jlong Init(JNIEnv* env,
           const base::android::JavaParamRef<jobject>& obj,
           const base::android::JavaParamRef<jobject>& jweb_contents) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents)
      << "A JoystickScrollProvider should be created with a valid WebContents.";

  DCHECK(!web_contents->GetUserData(kJoystickScrollUserDataKey))
      << "WebContents already has JoystickScrollProvider attached";

  JoystickScrollProvider* native_object =
      new JoystickScrollProvider(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(native_object);
}

JoystickScrollProvider::JoystickScrollProvider(JNIEnv* env,
                                               const JavaRef<jobject>& obj,
                                               WebContentsImpl* web_contents)
    : java_ref_(env, obj), web_contents_(web_contents) {
  web_contents_->SetUserData(kJoystickScrollUserDataKey, new UserData(this));
}

JoystickScrollProvider::~JoystickScrollProvider() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_JoystickScrollProvider_onNativeObjectDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void JoystickScrollProvider::ScrollBy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong time_ms,
    jfloat dx_dip,
    jfloat dy_dip) {
  if (!web_contents_)
    return;

  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      web_contents_->GetRenderWidgetHostView());
  if (!rwhv)
    return;

  if (!dx_dip && !dy_dip)
    return;

  // Revert the direction for syntheric mouse wheel event.
  blink::WebMouseWheelEvent event = WebMouseWheelEventBuilder::Build(
      -dx_dip, -dy_dip, 1.0, time_ms / MILLISECONDS_IN_SECOND, 0, 0);

  rwhv->SendMouseWheelEvent(event);
}

bool RegisterJoystickScrollProvider(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
