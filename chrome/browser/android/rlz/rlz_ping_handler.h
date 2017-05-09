// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RLZ_RLZ_PING_HANDLER_H_
#define CHROME_BROWSER_ANDROID_RLZ_RLZ_PING_HANDLER_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace chrome {
namespace android {

// JNI bridge for   RlzPingHandler.java
class RlzPingHandler : public net::URLFetcherDelegate {
 public:
  explicit RlzPingHandler(jobject jprofile);
  ~RlzPingHandler() override;

  // Makes a GET request to the designated web end point with the given
  // parameters. |j_brand| is a 4 character priorly designated brand value.
  // |j_language| is the 2 letter lower case language. |events| is a single
  // string where multiple 4 character long events are concatenated with ,
  // and |id| is a unique id for the device that is 50 characters long.
  void Ping(const base::android::JavaParamRef<jstring>& j_brand,
            const base::android::JavaParamRef<jstring>& j_language,
            const base::android::JavaParamRef<jstring>& j_events,
            const base::android::JavaParamRef<jstring>& j_id,
            const base::android::JavaParamRef<jobject>& j_callback);

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  std::unique_ptr<net::URLFetcher> url_fetcher_;
  base::android::ScopedJavaGlobalRef<jobject> j_callback_;

  DISALLOW_COPY_AND_ASSIGN(RlzPingHandler);
};

bool RegisterRlzPingHandler(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_RLZ_RLZ_PING_HANDLER_H_
