// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/context_selection_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/supports_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContextSelectionClient_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {
const void* const kContextSelectionClientUDKey = &kContextSelectionClientUDKey;

// This class deletes ContextSelectionClient when WebContents is destroyed.
class UserData : public base::SupportsUserData::Data {
 public:
  explicit UserData(ContextSelectionClient* client) : client_(client) {}

 private:
  std::unique_ptr<ContextSelectionClient> client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UserData);
};
}

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents)
      << "A ContextSelectionClient should be created with a valid WebContents.";

  return reinterpret_cast<intptr_t>(
      new ContextSelectionClient(env, obj, web_contents));
}

ContextSelectionClient::ContextSelectionClient(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    WebContents* web_contents)
    : java_ref_(env, obj),
      web_contents_(web_contents),
      weak_ptr_factory_(this) {
  DCHECK(!web_contents_->GetUserData(kContextSelectionClientUDKey));
  web_contents_->SetUserData(kContextSelectionClientUDKey, new UserData(this));
}

ContextSelectionClient::~ContextSelectionClient() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    Java_ContextSelectionClient_onNativeSideDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void ContextSelectionClient::RequestSurroundingText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int num_extra_characters,
    int callback_data) {
  RenderFrameHost* focused_frame = web_contents_->GetFocusedFrame();
  if (!focused_frame) {
    OnSurroundingTextReceived(callback_data, base::string16(), 0, 0);
    return;
  }

  focused_frame->RequestTextSurroundingSelection(
      base::Bind(&ContextSelectionClient::OnSurroundingTextReceived,
                 weak_ptr_factory_.GetWeakPtr(), callback_data),
      num_extra_characters);
}

void ContextSelectionClient::CancelAllRequests(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ContextSelectionClient::OnSurroundingTextReceived(
    int callback_data,
    const base::string16& text,
    int start,
    int end) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    ScopedJavaLocalRef<jstring> j_text = ConvertUTF16ToJavaString(env, text);
    Java_ContextSelectionClient_onSurroundingTextReceived(
        env, obj, callback_data, j_text, start, end);
  }
}

bool RegisterContextSelectionClient(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
