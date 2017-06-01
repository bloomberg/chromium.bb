// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/autofill_provider_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/android/form_data_android.h"
#include "components/autofill/core/browser/autofill_handler_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/AutofillProvider_jni.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::WebContents;
using gfx::RectF;

namespace autofill {

const int kInvalidRequestId = -1;

AutofillProviderAndroid::AutofillProviderAndroid(
    const JavaRef<jobject>& jcaller,
    content::WebContents* web_contents)
    : id_(kInvalidRequestId), web_contents_(web_contents) {
  JNIEnv* env = AttachCurrentThread();
  java_ref_ = JavaObjectWeakGlobalRef(env, jcaller);
  Java_AutofillProvider_setNativeAutofillProvider(
      env, jcaller, reinterpret_cast<jlong>(this));
}

AutofillProviderAndroid::~AutofillProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Remove the reference to this object on the Java side.
  Java_AutofillProvider_setNativeAutofillProvider(env, obj, 0);
}

void AutofillProviderAndroid::OnQueryFormFieldAutofill(
    AutofillHandlerProxy* handler,
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  // The id isn't passed to Java side because Android API guarantees the
  // response is always for current session, so we just use the current id
  // in response, see OnAutofillAvailable.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  id_ = id;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  form_ = base::MakeUnique<FormDataAndroid>(form);

  size_t index;
  if (!form_->GetFieldIndex(field, &index))
    return;

  gfx::Rect client_area = web_contents_->GetContainerBounds();
  gfx::RectF transformed_bounding =
      bounding_box + client_area.OffsetFromOrigin();

  ScopedJavaLocalRef<jobject> form_obj = form_->GetJavaPeer();
  handler_ = handler->GetWeakPtr();
  Java_AutofillProvider_startAutofillSession(
      env, obj, form_obj, index, transformed_bounding.x(),
      transformed_bounding.y(), transformed_bounding.width(),
      transformed_bounding.height());
}

void AutofillProviderAndroid::OnAutofillAvailable(JNIEnv* env,
                                                  jobject jcaller,
                                                  jobject formData) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler_) {
    const FormData& form = form_->GetAutofillValues();
    SendFormDataToRenderer(handler_.get(), id_, form);
  }
}

void AutofillProviderAndroid::OnTextFieldDidChange(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateHandler(handler) || !form_->SimilarFormAs(form))
    return;

  size_t index;
  if (!form_->GetSimilarFieldIndex(field, &index))
    return;

  form_->OnTextFieldDidChange(index, field.value);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onTextFieldDidChange(env, obj, index);
}

bool AutofillProviderAndroid::OnWillSubmitForm(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateHandler(handler) || !form_->SimilarFormAs(form))
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  Java_AutofillProvider_onWillSubmitForm(env, obj);
  return true;
}

void AutofillProviderAndroid::OnFocusNoLongerOnForm(
    AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateHandler(handler))
    return;

  OnFocusChanged(false, 0, RectF());
}

void AutofillProviderAndroid::OnFocusChanged(bool focus_on_form,
                                             size_t index,
                                             const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onFocusChanged(
      env, obj, focus_on_form, index, bounding_box.x(), bounding_box.y(),
      bounding_box.width(), bounding_box.height());
}

void AutofillProviderAndroid::OnDidFillAutofillFormData(
    AutofillHandlerProxy* handler,
    const FormData& form,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler != handler_.get() || !form_ || !form_->SimilarFormAs(form))
    return;

  for (const FormFieldData& field : form.fields) {
    if (!field.is_autofilled)
      continue;
    OnTextFieldDidChange(handler, form, field, timestamp);
  }
}

void AutofillProviderAndroid::Reset(AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler == handler_.get()) {
    handler_.reset();
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return;

    Java_AutofillProvider_reset(env, obj);
  }
}

bool AutofillProviderAndroid::ValidateHandler(AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool ret = handler == handler_.get();
  if (!ret)
    handler_.reset();
  return ret;
}

bool RegisterAutofillProvider(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
