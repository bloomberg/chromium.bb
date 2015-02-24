// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/credit_card_scanner_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/credit_card_scanner_view_delegate.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/CreditCardScanner_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace autofill {

// static
bool CreditCardScannerView::CanShow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> java_object(
      Java_CreditCardScanner_create(
          env, 0, base::android::GetApplicationContext(), 0));
  return Java_CreditCardScanner_canScan(env, java_object.obj());
}

// static
scoped_ptr<CreditCardScannerView> CreditCardScannerView::Create(
    const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
    content::WebContents* web_contents) {
  return make_scoped_ptr(
      new CreditCardScannerViewAndroid(delegate, web_contents));
}

// static
bool CreditCardScannerViewAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CreditCardScannerViewAndroid::CreditCardScannerViewAndroid(
    const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
    content::WebContents* web_contents)
    : delegate_(delegate),
      java_object_(Java_CreditCardScanner_create(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          base::android::GetApplicationContext(),
          WindowAndroidHelper::FromWebContents(web_contents)
              ->GetWindowAndroid()->GetJavaObject().obj())) {}

CreditCardScannerViewAndroid::~CreditCardScannerViewAndroid() {}

void CreditCardScannerViewAndroid::ScanCancelled(JNIEnv* env, jobject object) {
  delegate_->ScanCancelled();
}

void CreditCardScannerViewAndroid::ScanCompleted(JNIEnv* env,
                                                 jobject object,
                                                 jstring card_number,
                                                 jint expiration_month,
                                                 jint expiration_year) {
  delegate_->ScanCompleted(
      base::android::ConvertJavaStringToUTF16(env, card_number),
      static_cast<int>(expiration_month), static_cast<int>(expiration_year));
}

void CreditCardScannerViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CreditCardScanner_scan(env, java_object_.obj());
}

}  // namespace autofill
