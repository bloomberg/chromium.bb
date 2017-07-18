// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/origin_verifier.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/values.h"
#include "chrome/browser/android/digital_asset_links/digital_asset_links_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "jni/OriginVerifier_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

namespace customtabs {

OriginVerifier::OriginVerifier(JNIEnv* env, jobject obj, jobject jprofile) {
  jobject_.Reset(env, obj);
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  asset_link_handler_ =
      base::MakeUnique<digital_asset_links::DigitalAssetLinksHandler>(
          profile->GetRequestContext());
}

OriginVerifier::~OriginVerifier() = default;

bool OriginVerifier::VerifyOrigin(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jstring>& j_package_name,
                                  const JavaParamRef<jstring>& j_fingerprint,
                                  const JavaParamRef<jstring>& j_origin) {
  if (!j_package_name || !j_fingerprint || !j_origin)
    return false;

  std::string package_name = ConvertJavaStringToUTF8(env, j_package_name);
  std::string fingerprint = ConvertJavaStringToUTF8(env, j_fingerprint);
  std::string origin = ConvertJavaStringToUTF8(env, j_origin);

  // Multiple calls here will end up resetting the callback on the handler side
  // and cancelling previous requests.
  // If during the request this verifier gets killed, the handler and the
  // UrlFetcher making the request will also get killed, so we won't get any
  // dangling callback reference issues.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return asset_link_handler_->CheckDigitalAssetLinkRelationship(
      base::Bind(&customtabs::OriginVerifier::OnRelationshipCheckComplete,
                 base::Unretained(this)),
      origin, package_name, fingerprint,
      "delegate_permission/common.use_as_origin");
}

void OriginVerifier::OnRelationshipCheckComplete(
    std::unique_ptr<base::DictionaryValue> response) {
  JNIEnv* env = base::android::AttachCurrentThread();

  bool verified = false;

  if (response) {
    response->GetBoolean(
        digital_asset_links::kDigitalAssetLinksCheckResponseKeyLinked,
        &verified);
  }
  Java_OriginVerifier_originVerified(env, jobject_, verified);
}

void OriginVerifier::Destroy(JNIEnv* env,
                             const base::android::JavaRef<jobject>& obj) {
  delete this;
}

static jlong Init(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& jprofile) {
  if (!g_browser_process)
    return 0;

  OriginVerifier* native_verifier = new OriginVerifier(env, obj, jprofile);
  return reinterpret_cast<intptr_t>(native_verifier);
}

}  // namespace customtabs
