// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_model_android.h"

#include "base/logging.h"
#include "chrome/browser/ssl/security_state_model.h"
#include "content/public/browser/web_contents.h"
#include "jni/SecurityStateModel_jni.h"

// static
bool RegisterSecurityStateModelAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jint GetSecurityLevelForWebContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  SecurityStateModel::CreateForWebContents(web_contents);
  SecurityStateModel* model = SecurityStateModel::FromWebContents(web_contents);
  DCHECK(model);
  return model->GetSecurityInfo().security_level;
}

// static
jboolean IsDeprecatedSHA1Present(JNIEnv* env,
                                 const JavaParamRef<jclass>& jcaller,
                                 const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  SecurityStateModel::CreateForWebContents(web_contents);
  SecurityStateModel* model = SecurityStateModel::FromWebContents(web_contents);
  DCHECK(model);
  return model->GetSecurityInfo().sha1_deprecation_status !=
         SecurityStateModel::NO_DEPRECATED_SHA1;
}

// static
jboolean IsPassiveMixedContentPresent(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  SecurityStateModel::CreateForWebContents(web_contents);
  SecurityStateModel* model = SecurityStateModel::FromWebContents(web_contents);
  DCHECK(model);
  return model->GetSecurityInfo().mixed_content_status ==
             SecurityStateModel::DISPLAYED_MIXED_CONTENT ||
         model->GetSecurityInfo().mixed_content_status ==
             SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;
}
