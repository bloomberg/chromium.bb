// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/autofill_provider_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/android/form_data_android.h"
#include "components/autofill/core/browser/autofill_handler_proxy.h"
#include "components/autofill/core/common/autofill_constants.h"
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

AutofillProviderAndroid::AutofillProviderAndroid(
    const JavaRef<jobject>& jcaller,
    content::WebContents* web_contents)
    : id_(kNoQueryId), web_contents_(web_contents) {
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

  // Only start a new session when form is changed, the focus or feild value
  // change will also trigger the query, so it is safe to ignore the query
  // for the same form.
  if (IsCurrentlyLinkedForm(form)) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  form_ = base::MakeUnique<FormDataAndroid>(form);

  size_t index;
  if (!form_->GetFieldIndex(field, &index))
    return;

  gfx::RectF transformed_bounding = ToClientAreaBound(bounding_box);

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
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t index;
  if (!ValidateHandler(handler) || !IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &index))
    return;

  form_->OnTextFieldDidChange(index, field.value);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  gfx::RectF transformed_bounding = ToClientAreaBound(bounding_box);
  Java_AutofillProvider_onTextFieldDidChange(
      env, obj, index, transformed_bounding.x(), transformed_bounding.y(),
      transformed_bounding.width(), transformed_bounding.height());
}

bool AutofillProviderAndroid::OnWillSubmitForm(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateHandler(handler) || !IsCurrentlyLinkedForm(form))
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

void AutofillProviderAndroid::OnFocusOnFormField(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  size_t index;
  if (!ValidateHandler(handler) || !IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &index))
    return;

  // Because this will trigger a suggestion query, set request id to browser
  // initiated request.
  id_ = kNoQueryId;

  OnFocusChanged(true, index, ToClientAreaBound(bounding_box));
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
  if (handler != handler_.get() || !IsCurrentlyLinkedForm(form))
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onDidFillAutofillFormData(env, obj);
}

void AutofillProviderAndroid::Reset(AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler == handler_.get()) {
    handler_.reset();
    form_.reset(nullptr);
    id_ = kNoQueryId;

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

bool AutofillProviderAndroid::IsCurrentlyLinkedForm(const FormData& form) {
  return form_ && form_->SimilarFormAs(form);
}

gfx::RectF AutofillProviderAndroid::ToClientAreaBound(
    const gfx::RectF& bounding_box) {
  gfx::Rect client_area = web_contents_->GetContainerBounds();
  return bounding_box + client_area.OffsetFromOrigin();
}

bool RegisterAutofillProvider(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
