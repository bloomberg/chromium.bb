// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

class TemplateURL;


// Android wrapper of the TemplateUrlService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper.
class TemplateUrlServiceAndroid : public TemplateURLServiceObserver {
 public:
  TemplateUrlServiceAndroid(JNIEnv* env, jobject obj);

  void Load(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetUserSelectedDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint selected_index);
  jint GetDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetTemplateUrlCount(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  jboolean IsLoaded(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetPrepopulatedTemplateUrlAt(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index);
  jboolean IsSearchProviderManaged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsSearchByImageAvailable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsDefaultSearchEngineGoogle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetUrlForSearchQuery(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery);
  base::android::ScopedJavaLocalRef<jstring> GetUrlForVoiceSearchQuery(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery);
  base::android::ScopedJavaLocalRef<jstring> ReplaceSearchTermsInUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery,
      const base::android::JavaParamRef<jstring>& jcurrent_url);
  base::android::ScopedJavaLocalRef<jstring> GetUrlForContextualSearchQuery(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery,
      const base::android::JavaParamRef<jstring>& jalternate_term,
      jboolean jshould_prefetch,
      const base::android::JavaParamRef<jstring>& jprotocol_version);
  base::android::ScopedJavaLocalRef<jstring> GetSearchEngineUrlFromTemplateUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index);

  static bool Register(JNIEnv* env);

 private:
  ~TemplateUrlServiceAndroid() override;

  bool IsPrepopulatedTemplate(TemplateURL* url);

  void OnTemplateURLServiceLoaded();

  // TemplateUrlServiceObserver:
  void OnTemplateURLServiceChanged() override;

  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the TemplateUrlService for the main profile.
  TemplateURLService* template_url_service_;

  scoped_ptr<TemplateURLService::Subscription> template_url_subscription_;

  DISALLOW_COPY_AND_ASSIGN(TemplateUrlServiceAndroid);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_
