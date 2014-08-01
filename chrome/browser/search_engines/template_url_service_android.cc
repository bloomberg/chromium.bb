// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_android.h"

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/browser/google_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "jni/TemplateUrlService_jni.h"
#include "net/base/url_util.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

Profile* GetOriginalProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

}  // namespace

TemplateUrlServiceAndroid::TemplateUrlServiceAndroid(JNIEnv* env,
                                                     jobject obj)
    : weak_java_obj_(env, obj),
      template_url_service_(
          TemplateURLServiceFactory::GetForProfile(GetOriginalProfile())) {
  template_url_subscription_ =
      template_url_service_->RegisterOnLoadedCallback(
          base::Bind(&TemplateUrlServiceAndroid::OnTemplateURLServiceLoaded,
                     base::Unretained(this)));
}

TemplateUrlServiceAndroid::~TemplateUrlServiceAndroid() {
}

void TemplateUrlServiceAndroid::Load(JNIEnv* env, jobject obj) {
  template_url_service_->Load();
}

void TemplateUrlServiceAndroid::SetUserSelectedDefaultSearchProvider(
    JNIEnv* env,
    jobject obj,
    jint selected_index) {
  std::vector<TemplateURL*> template_urls =
      template_url_service_->GetTemplateURLs();
  size_t selected_index_size_t = static_cast<size_t>(selected_index);
  DCHECK_LT(selected_index_size_t, template_urls.size()) <<
      "Wrong index for search engine";

  TemplateURL* template_url = template_urls[selected_index_size_t];
  DCHECK_GT(template_url->prepopulate_id(), 0) <<
      "Tried to select non-prepopulated search engine";
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
}

jint TemplateUrlServiceAndroid::GetDefaultSearchProvider(JNIEnv* env,
                                                         jobject obj) {
  std::vector<TemplateURL*> template_urls =
      template_url_service_->GetTemplateURLs();
  TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  for (size_t i = 0; i < template_urls.size(); ++i) {
    if (default_search_provider == template_urls[i])
      return i;
  }
  return -1;
}

jboolean TemplateUrlServiceAndroid::IsLoaded(JNIEnv* env, jobject obj) {
  return template_url_service_->loaded();
}

jint TemplateUrlServiceAndroid::GetTemplateUrlCount(JNIEnv* env, jobject obj) {
  return template_url_service_->GetTemplateURLs().size();
}

jboolean TemplateUrlServiceAndroid::IsSearchProviderManaged(JNIEnv* env,
                                                            jobject obj) {
  return template_url_service_->is_default_search_managed();
}

jboolean TemplateUrlServiceAndroid::IsSearchByImageAvailable(JNIEnv* env,
                                                             jobject obj) {
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  return default_search_provider &&
      !default_search_provider->image_url().empty() &&
      default_search_provider->image_url_ref().IsValid(
          template_url_service_->search_terms_data());
}

jboolean TemplateUrlServiceAndroid::IsDefaultSearchEngineGoogle(JNIEnv* env,
                                                                jobject obj) {
  TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  return default_search_provider &&
      default_search_provider->url_ref().HasGoogleBaseURLs(
          template_url_service_->search_terms_data());
}

base::android::ScopedJavaLocalRef<jobject>
TemplateUrlServiceAndroid::GetPrepopulatedTemplateUrlAt(JNIEnv* env,
                                                        jobject obj,
                                                        jint index) {
  TemplateURL* template_url = template_url_service_->GetTemplateURLs()[index];
  if (!IsPrepopulatedTemplate(template_url) &&
      !template_url->created_by_policy())
   return ScopedJavaLocalRef<jobject>();

  return Java_TemplateUrl_create(
      env,
      index,
      ConvertUTF16ToJavaString(env, template_url->short_name()).obj(),
      ConvertUTF16ToJavaString(env, template_url->keyword()).obj());
}

bool TemplateUrlServiceAndroid::IsPrepopulatedTemplate(TemplateURL* url) {
  return url->prepopulate_id() > 0;
}

void TemplateUrlServiceAndroid::OnTemplateURLServiceLoaded() {
  template_url_subscription_.reset();
  JNIEnv* env = base::android::AttachCurrentThread();
  if (weak_java_obj_.get(env).is_null())
    return;

  Java_TemplateUrlService_templateUrlServiceLoaded(
      env, weak_java_obj_.get(env).obj());
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::GetUrlForSearchQuery(JNIEnv* env,
                                                jobject obj,
                                                jstring jquery) {
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();

  base::string16 query(ConvertJavaStringToUTF16(env, jquery));

  std::string url;
  if (default_provider &&
      default_provider->url_ref().SupportsReplacement(
          template_url_service_->search_terms_data()) &&
      !query.empty()) {
    url = default_provider->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(query),
        template_url_service_->search_terms_data());
  }

  return ConvertUTF8ToJavaString(env, url);
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::GetUrlForVoiceSearchQuery(JNIEnv* env,
                                                     jobject obj,
                                                     jstring jquery) {
  base::string16 query(ConvertJavaStringToUTF16(env, jquery));
  std::string url;

  if (!query.empty()) {
    GURL gurl(GetDefaultSearchURLForSearchTerms(template_url_service_, query));
    if (google_util::IsGoogleSearchUrl(gurl))
      gurl = net::AppendQueryParameter(gurl, "inm", "vs");
    url = gurl.spec();
  }

  return ConvertUTF8ToJavaString(env, url);
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::ReplaceSearchTermsInUrl(JNIEnv* env,
                                                   jobject obj,
                                                   jstring jquery,
                                                   jstring jcurrent_url) {
  TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();

  base::string16 query(ConvertJavaStringToUTF16(env, jquery));
  GURL current_url(ConvertJavaStringToUTF16(env, jcurrent_url));
  GURL destination_url(current_url);
  if (default_provider && !query.empty()) {
    bool refined_query = default_provider->ReplaceSearchTermsInURL(
        current_url, TemplateURLRef::SearchTermsArgs(query),
        template_url_service_->search_terms_data(), &destination_url);
    if (refined_query)
      return ConvertUTF8ToJavaString(env, destination_url.spec());
  }
  return base::android::ScopedJavaLocalRef<jstring>(env, NULL);
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::GetUrlForContextualSearchQuery(JNIEnv* env,
                                                          jobject obj,
                                                          jstring jquery) {
  base::string16 query(ConvertJavaStringToUTF16(env, jquery));
  std::string url;

  if (!query.empty()) {
    GURL gurl(GetDefaultSearchURLForSearchTerms(template_url_service_, query));
    if (google_util::IsGoogleSearchUrl(gurl)) {
      gurl = net::AppendQueryParameter(gurl, "ctxs", "1");
      // Indicate that the search page is being prefetched.
      gurl = net::AppendQueryParameter(gurl, "pf", "c");
    }
    url = gurl.spec();
  }

  return ConvertUTF8ToJavaString(env, url);
}

static jlong Init(JNIEnv* env, jobject obj) {
  TemplateUrlServiceAndroid* template_url_service_android =
      new TemplateUrlServiceAndroid(env, obj);
  return reinterpret_cast<intptr_t>(template_url_service_android);
}

// static
bool TemplateUrlServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
