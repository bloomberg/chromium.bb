// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sdch_test_util.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/macros.h"
#include "components/cronet/android/test/cronet_test_util.h"
#include "jni/SdchObserver_jni.h"
#include "net/base/sdch_manager.h"
#include "net/base/sdch_observer.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace cronet {

namespace {

class TestSdchObserver : public net::SdchObserver {
 public:
  TestSdchObserver(
      const GURL& target_url,
      net::SdchManager* manager,
      const base::android::ScopedJavaGlobalRef<jobject>& jsdch_observer_ref)
      : target_url_(target_url), manager_(manager) {
    jsdch_observer_ref_.Reset(jsdch_observer_ref);
  }

  // SdchObserver implementation
  void OnDictionaryAdded(const GURL& dictionary_url,
                         const std::string& server_hash) override {
    // Only notify if the dictionary for the |target_url_| has been added.
    if (manager_->GetDictionarySet(target_url_)) {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_SdchObserver_onDictionaryAdded(env, jsdch_observer_ref_);
      manager_->RemoveObserver(this);
      delete this;
    }
  }

  void OnDictionaryRemoved(const std::string& server_hash) override {}

  void OnDictionaryUsed(const std::string& server_hash) override {}

  void OnGetDictionary(const GURL& request_url,
                       const GURL& dictionary_url) override {}

  void OnClearDictionaries() override {}

 private:
  GURL target_url_;
  net::SdchManager* manager_;
  base::android::ScopedJavaGlobalRef<jobject> jsdch_observer_ref_;

  DISALLOW_COPY_AND_ASSIGN(TestSdchObserver);
};

void AddSdchObserverHelper(
    const GURL& target_url,
    const base::android::ScopedJavaGlobalRef<jobject>& jsdch_observer_ref,
    jlong jadapter) {
  JNIEnv* env = base::android::AttachCurrentThread();
  net::URLRequestContext* url_request_context =
      TestUtil::GetURLRequestContext(jadapter);
  // If dictionaries for |target_url| are already added, skip adding the
  // observer.
  if (url_request_context->sdch_manager()->GetDictionarySet(target_url)) {
    Java_SdchObserver_onDictionarySetAlreadyPresent(env, jsdch_observer_ref);
    return;
  }

  url_request_context->sdch_manager()->AddObserver(new TestSdchObserver(
      target_url, url_request_context->sdch_manager(), jsdch_observer_ref));
  Java_SdchObserver_onAddSdchObserverCompleted(env, jsdch_observer_ref);
}

}  // namespace

void AddSdchObserver(JNIEnv* env,
                     const JavaParamRef<jobject>& jsdch_observer,
                     const JavaParamRef<jstring>& jtarget_url,
                     jlong jadapter) {
  base::android::ScopedJavaGlobalRef<jobject> jsdch_observer_ref;
  // ScopedJavaGlobalRef do not hold onto the env reference, so it is safe to
  // use it across threads. |AddSdchObserverHelper| will acquire a new
  // JNIEnv before calling into Java.
  jsdch_observer_ref.Reset(env, jsdch_observer);

  GURL target_url(base::android::ConvertJavaStringToUTF8(env, jtarget_url));
  TestUtil::RunAfterContextInit(jadapter,
                                base::Bind(&AddSdchObserverHelper, target_url,
                                           jsdch_observer_ref, jadapter));
}

bool RegisterSdchTestUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
