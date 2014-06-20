// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_IMPL_H_

#include "android_webview/browser/aw_contents_io_thread_client.h"

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

class GURL;

namespace content {
class ResourceRequestInfo;
class WebContents;
}

namespace net {
class URLRequest;
}

namespace android_webview {

class AwWebResourceResponse;

class AwContentsIoThreadClientImpl : public AwContentsIoThreadClient {
 public:
   // Called when AwContents is created before there is a Java client.
  static void RegisterPendingContents(content::WebContents* web_contents);

  // Associates the |jclient| instance (which must implement the
  // AwContentsIoThreadClient Java interface) with the |web_contents|.
  // This should be called at most once per |web_contents|.
  static void Associate(content::WebContents* web_contents,
                        const base::android::JavaRef<jobject>& jclient);

  // Either |pending_associate| is true or |jclient| holds a non-null
  // Java object.
  AwContentsIoThreadClientImpl(bool pending_associate,
                               const base::android::JavaRef<jobject>& jclient);
  virtual ~AwContentsIoThreadClientImpl() OVERRIDE;

  // Implementation of AwContentsIoThreadClient.
  virtual bool PendingAssociation() const OVERRIDE;
  virtual CacheMode GetCacheMode() const OVERRIDE;
  virtual scoped_ptr<AwWebResourceResponse> ShouldInterceptRequest(
      const GURL& location,
      const net::URLRequest* request) OVERRIDE;
  virtual bool ShouldBlockContentUrls() const OVERRIDE;
  virtual bool ShouldBlockFileUrls() const OVERRIDE;
  virtual bool ShouldAcceptThirdPartyCookies() const OVERRIDE;
  virtual bool ShouldBlockNetworkLoads() const OVERRIDE;
  virtual void NewDownload(const GURL& url,
                           const std::string& user_agent,
                           const std::string& content_disposition,
                           const std::string& mime_type,
                           int64 content_length) OVERRIDE;
  virtual void NewLoginRequest(const std::string& realm,
                               const std::string& account,
                               const std::string& args) OVERRIDE;

 private:
  bool pending_association_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AwContentsIoThreadClientImpl);
};

// JNI registration method.
bool RegisterAwContentsIoThreadClientImpl(JNIEnv* env);

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_IMPL_H_
