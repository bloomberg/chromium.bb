// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_external_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/jni_string.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/AutofillExternalDelegate_jni.h"
#include "ui/gfx/android/window_android.h"
#include "ui/gfx/rect.h"

using base::android::MethodID;
using content::BrowserThread;

void AutofillExternalDelegate::CreateForWebContentsAndManager(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      UserDataKey(),
      new AutofillExternalDelegateAndroid(web_contents, autofill_manager));
}

AutofillExternalDelegateAndroid::AutofillExternalDelegateAndroid(
    content::WebContents* web_contents, AutofillManager* manager)
    : AutofillExternalDelegate(web_contents, manager) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::FromWebContents(web_contents);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> container_view_delegate;
  // content_view_core should only be NULL for testing.
  if (content_view_core) {
    container_view_delegate = content_view_core->GetContainerViewDelegate();
  }

  java_object_.Reset(Java_AutofillExternalDelegate_create(env,
      reinterpret_cast<jint>(this), container_view_delegate.obj()));
}

AutofillExternalDelegateAndroid::~AutofillExternalDelegateAndroid() {}

void AutofillExternalDelegateAndroid::AutofillPopupSelected(JNIEnv *env,
                                                            jobject obj,
                                                            jint list_index,
                                                            jstring value,
                                                            jint unique_id) {
  string16 value_utf16 = base::android::ConvertJavaStringToUTF16(env, value);
  DidAcceptAutofillSuggestions(value_utf16, unique_id, list_index);
}

void AutofillExternalDelegateAndroid::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need an array of AutofillSuggestion.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jclass> autofill_suggestion_clazz =
      base::android::GetClass(env,
          "org/chromium/chrome/browser/autofill/AutofillSuggestion");
  ScopedJavaLocalRef<jobjectArray> data_array(env,
      env->NewObjectArray(autofill_values.size(),
                          autofill_suggestion_clazz.obj(), NULL));
  base::android::CheckException(env);
  for (size_t i = 0; i < autofill_values.size(); ++i) {
    ScopedJavaLocalRef<jstring> value =
        base::android::ConvertUTF16ToJavaString(env, autofill_values[i]);
    ScopedJavaLocalRef<jstring> label =
        base::android::ConvertUTF16ToJavaString(env, autofill_labels[i]);
    int unique_id = autofill_unique_ids[i];
    ScopedJavaLocalRef<jobject> data =
        Java_AutofillExternalDelegate_createAutofillSuggestion(env,
                                                               value.obj(),
                                                               label.obj(),
                                                               unique_id);
    env->SetObjectArrayElement(data_array.obj(), i, data.obj());
    base::android::CheckException(env);
  }
  ui::WindowAndroid* window_android =
      WindowAndroidHelper::FromWebContents(web_contents())->GetWindowAndroid();
  Java_AutofillExternalDelegate_openAutofillPopup(env, java_object_.obj(),
      window_android->GetJavaObject().obj(), data_array.obj());
}

void AutofillExternalDelegateAndroid::HideAutofillPopupInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillExternalDelegate_closeAutofillPopup(env, java_object_.obj());
}

void AutofillExternalDelegateAndroid::SetBounds(const gfx::Rect& bounds) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillExternalDelegate_setAutofillAnchorRect(env,
                                                      java_object_.obj(),
                                                      bounds.x(),
                                                      bounds.y(),
                                                      bounds.width(),
                                                      bounds.height());
}

// static
bool AutofillExternalDelegateAndroid::RegisterAutofillExternalDelegate(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
