// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
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
      const base::android::JavaParamRef<jstring>& jkeyword);

  jint GetDefaultSearchProviderIndex(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  jint GetTemplateUrlCount(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;
  jboolean IsLoaded(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;
  base::android::ScopedJavaLocalRef<jobject> GetTemplateUrlAt(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index) const;
  jboolean IsDefaultSearchManaged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsSearchByImageAvailable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsDefaultSearchEngineGoogle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean DoesDefaultSearchEngineHaveLogo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsSearchResultsPageFromDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl);
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
      const base::android::JavaParamRef<jstring>& jkeyword);

  // Adds a custom search engine, sets |jkeyword| as its short_name and keyword,
  // and sets its date_created as |age_in_days| days before the current time.
  base::android::ScopedJavaLocalRef<jstring> AddSearchEngineForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword,
      jint age_in_days);

  // Finds the search engine whose keyword matches |jkeyword| and sets its
  // last_visited time as the current time.
  base::android::ScopedJavaLocalRef<jstring> UpdateLastVisitedForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword);

  static bool Register(JNIEnv* env);

 private:
  ~TemplateUrlServiceAndroid() override;

  void OnTemplateURLServiceLoaded();

  // TemplateUrlServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Updates |template_urls_| to contain all TemplateURLs.  It sorts this list
  // with prepopulated engines first, then any default non-prepopulated engine,
  // then other non-prepopulated engines based on last_visited in descending
  // order.
  void LoadTemplateURLs();

  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the TemplateUrlService for the main profile.
  TemplateURLService* template_url_service_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  // Caches the up-to-date TemplateURL list so that calls from Android could
  // directly get data from it.
  std::vector<TemplateURL*> template_urls_;

  DISALLOW_COPY_AND_ASSIGN(TemplateUrlServiceAndroid);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_
