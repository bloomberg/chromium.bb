// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_IMPL_H_

#include <stdint.h>

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace content {
class WebContents;
}

namespace net {
class URLRequest;
}

namespace android_webview {

class AwContentsIoThreadClientImpl : public AwContentsIoThreadClient {
 public:
   // Called when AwContents is created before there is a Java client.
  static void RegisterPendingContents(content::WebContents* web_contents);

  // Associates the |jclient| instance (which must implement the
  // AwContentsIoThreadClient Java interface) with the |web_contents|.
  // This should be called at most once per |web_contents|.
  static void Associate(content::WebContents* web_contents,
                        const base::android::JavaRef<jobject>& jclient);

  // Sets the |jclient| java instance to which service worker related
  // callbacks should be delegated.
  static void SetServiceWorkerIoThreadClient(
      const base::android::JavaRef<jobject>& jclient,
      const base::android::JavaRef<jobject>& browser_context);

  // Either |pending_associate| is true or |jclient| holds a non-null
  // Java object.
  AwContentsIoThreadClientImpl(bool pending_associate,
                               const base::android::JavaRef<jobject>& jclient);
  ~AwContentsIoThreadClientImpl() override;

  // Implementation of AwContentsIoThreadClient.
  bool PendingAssociation() const override;
  CacheMode GetCacheMode() const override;
  void ShouldInterceptRequestAsync(
      const net::URLRequest* request,
      const ShouldInterceptRequestResultCallback callback) override;
  bool ShouldBlockContentUrls() const override;
  bool ShouldBlockFileUrls() const override;
  bool ShouldAcceptThirdPartyCookies() const override;
  bool ShouldBlockNetworkLoads() const override;
  void OnReceivedError(const net::URLRequest* request) override;
  void OnReceivedHttpError(
      const net::URLRequest* request,
      const net::HttpResponseHeaders* response_headers) override;

 private:
  bool pending_association_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::android::ScopedJavaGlobalRef<jobject> bg_thread_client_object_;

  DISALLOW_COPY_AND_ASSIGN(AwContentsIoThreadClientImpl);
};

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_IMPL_H_
