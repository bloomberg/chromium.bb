// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/data_reduction_proxy_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_infobar_delegate_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/DataReductionProxyInfoBarDelegate_jni.h"

// DataReductionProxyInfoBar:

// static
void DataReductionProxyInfoBar::Launch(
    JNIEnv* env, jclass, jobject jweb_contents, jstring jlink_url) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  DataReductionProxyInfoBarDelegateAndroid::Create(
      web_contents, base::android::ConvertJavaStringToUTF8(env, jlink_url));
}

// static
bool DataReductionProxyInfoBar::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

DataReductionProxyInfoBar::DataReductionProxyInfoBar(
    scoped_ptr<DataReductionProxyInfoBarDelegateAndroid> delegate)
    : ConfirmInfoBar(delegate.Pass()), java_data_reduction_proxy_delegate_() {}

DataReductionProxyInfoBar::~DataReductionProxyInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionProxyInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_data_reduction_proxy_delegate_.Reset(
      Java_DataReductionProxyInfoBarDelegate_create(env));

  return Java_DataReductionProxyInfoBarDelegate_showDataReductionProxyInfoBar(
      env, java_data_reduction_proxy_delegate_.obj(), GetEnumeratedIconId());
}

DataReductionProxyInfoBarDelegateAndroid*
DataReductionProxyInfoBar::GetDelegate() {
  return static_cast<DataReductionProxyInfoBarDelegateAndroid*>(delegate());
}

// DataReductionProxyInfoBarDelegate:

// static
scoped_ptr<infobars::InfoBar>
DataReductionProxyInfoBarDelegateAndroid::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    scoped_ptr<DataReductionProxyInfoBarDelegateAndroid> delegate) {
  return scoped_ptr<infobars::InfoBar>(
      new DataReductionProxyInfoBar(delegate.Pass()));
}

// JNI for DataReductionProxyInfoBarDelegate.
void Launch(JNIEnv* env,
            const JavaParamRef<jclass>& clazz,
            const JavaParamRef<jobject>& jweb_contents,
            const JavaParamRef<jstring>& jlink_url) {
  DataReductionProxyInfoBar::Launch(env, clazz, jweb_contents, jlink_url);
}
