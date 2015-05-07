// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_url_request_job_factory.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/MockUrlRequestJobFactory_jni.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"

namespace cronet {

void AddUrlInterceptors(JNIEnv* env, jclass jcaller) {
  net::URLRequestMockDataJob::AddUrlHandler();
  net::URLRequestFailedJob::AddUrlHandler();
}

jstring GetMockUrlWithFailure(JNIEnv* jenv,
                              jclass jcaller,
                              jint jphase,
                              jint jnet_error) {
  GURL url(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      static_cast<net::URLRequestFailedJob::FailurePhase>(jphase),
      static_cast<int>(jnet_error)));
  return base::android::ConvertUTF8ToJavaString(jenv, url.spec()).Release();
}

jstring GetMockUrlForData(JNIEnv* jenv,
                          jclass jcaller,
                          jstring jdata,
                          jint jdata_repeat_count) {
  std::string data(base::android::ConvertJavaStringToUTF8(jenv, jdata));
  GURL url(net::URLRequestMockDataJob::GetMockHttpUrl(data,
                                                      jdata_repeat_count));
  return base::android::ConvertUTF8ToJavaString(jenv, url.spec()).Release();
}

bool RegisterMockUrlRequestJobFactory(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
