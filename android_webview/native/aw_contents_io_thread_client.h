// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_H_

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class GURL;
class InterceptedRequestData;
class JavaDelegateMapMaintainer;

namespace content {
class WebContents;
}

namespace net {
class URLRequest;
}

namespace android_webview {

// This class provides a means of calling Java methods on an instance that has
// a 1:1 relationship with a WebContents instance directly from the IO thread.
//
// Specifically this is used to implement URLRequest intercepting in a way that
// lets the embedder associate URLRequests with the WebContents that the
// URLRequest is made for.
//
// The native class is intended to be a short-lived handle that pins the
// Java-side instance. It is preferable to use the static getter methods to
// obtain a new instance of the class rather than holding on to one for
// prolonged periods of time (see note for more details).
//
// Note: The native AwContentsIoThreadClient instance has a Global ref to
// the Java object. By keeping the native AwContentsIoThreadClient
// instance alive you're also prolonging the lifetime of the Java instance, so
// don't keep a AwContentsIoThreadClient if you don't need to.
class AwContentsIoThreadClient {
 public:
  // This will attempt to fetch the AwContentsIoThreadClient for the given
  // |render_process_id|, |render_view_id| pair.
  // This method can be called from any thread.
  // An empty scoped_ptr is a valid return value.
  static AwContentsIoThreadClient FromID(int render_process_id,
                                         int render_view_id);

  // Associates the |jclient| instance (which must implement the
  // AwContentsIoThreadClient Java interface) with the |web_contents|.
  // This should be called at most once per |web_contents|.
  static void Associate(content::WebContents* web_contents,
                        const base::android::JavaRef<jobject>& jclient);

  AwContentsIoThreadClient(const AwContentsIoThreadClient& orig);
  ~AwContentsIoThreadClient();
  void operator=(const AwContentsIoThreadClient& rhs);

  // This method is called on the IO thread only.
  scoped_ptr<InterceptedRequestData> ShouldInterceptRequest(
      const net::URLRequest* request);

 private:
  AwContentsIoThreadClient();
  AwContentsIoThreadClient(const base::android::JavaRef<jobject>& jclient);

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

// JNI registration method.
bool RegisterAwContentsIoThreadClient(JNIEnv* env);


} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_IO_THREAD_CLIENT_H_
