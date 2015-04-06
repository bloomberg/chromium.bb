// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_

#include <jni.h>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class GrowableIOBuffer;
class HttpRequestHeaders;
class HttpResponseHeaders;
class UploadDataStream;
}  // namespace net

namespace cronet {

class CronetURLRequestContextAdapter;

bool CronetUrlRequestAdapterRegisterJni(JNIEnv* env);

// An adapter from Java CronetUrlRequest object to net::URLRequest.
// Created and configured from a Java thread. Start, ReadData, and Destroy are
// posted to network thread and all callbacks into the Java CronetUrlRequest are
// done on the network thread. Java CronetUrlRequest is expected to initiate the
// next step like FollowDeferredRedirect, ReadData or Destroy. Public methods
// can be called on any thread except PopulateResponseHeaders and Get* methods,
// which can only be called on the network thread.
class CronetURLRequestAdapter : public net::URLRequest::Delegate {
 public:
  CronetURLRequestAdapter(CronetURLRequestContextAdapter* context,
                          JNIEnv* env,
                          jobject jurl_request,
                          const GURL& url,
                          net::RequestPriority priority);
  ~CronetURLRequestAdapter() override;

  // Methods called prior to Start are never called on network thread.

  // Sets the request method GET, POST etc.
  jboolean SetHttpMethod(JNIEnv* env, jobject jcaller, jstring jmethod);

  // Adds a header to the request before it starts.
  jboolean AddRequestHeader(JNIEnv* env,
                            jobject jcaller,
                            jstring jname,
                            jstring jvalue);

  // Bypasses cache. If context is not set up to use cache, this call has no
  // effect.
  void DisableCache(JNIEnv* env, jobject jcaller);

  // Adds a request body to the request before it starts.
  void SetUpload(scoped_ptr<net::UploadDataStream> upload);

  // Starts the request.
  void Start(JNIEnv* env, jobject jcaller);

  // Follows redirect.
  void FollowDeferredRedirect(JNIEnv* env, jobject jcaller);

  // Reads more data.
  void ReadData(JNIEnv* env, jobject jcaller);

  // Releases all resources for the request and deletes the object itself.
  void Destroy(JNIEnv* env, jobject jcaller);

  // Populate response headers on network thread.
  void PopulateResponseHeaders(JNIEnv* env,
                               jobject jcaller,
                               jobject jheaders_list);

  // When called during a OnRedirect or OnResponseStarted callback, these
  // methods return the corresponding response information. These methods
  // can only be called on the network thread.

  // Gets http status text from the response headers.
  base::android::ScopedJavaLocalRef<jstring> GetHttpStatusText(
      JNIEnv* env,
      jobject jcaller) const;

  // Gets NPN or ALPN Negotiated Protocol (if any) from HttpResponseInfo.
  base::android::ScopedJavaLocalRef<jstring> GetNegotiatedProtocol(
      JNIEnv* env,
      jobject jcaller) const;

  // Returns true if response is coming from the cache.
  jboolean GetWasCached(JNIEnv* env, jobject jcaller) const;

  // Gets the total amount of body data received from network after
  // SSL/SPDY/QUIC decoding and proxy handling. Basically the size of the body
  // prior to decompression.
  int64 GetTotalReceivedBytes(JNIEnv* env, jobject jcaller) const;

  // net::URLRequest::Delegate implementations:

  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

 private:
  void StartOnNetworkThread();
  void FollowDeferredRedirectOnNetworkThread();
  void ReadDataOnNetworkThread();
  void DestroyOnNetworkThread();

  // Checks status of the request_adapter, return false if |is_success()| is
  // true, otherwise report error and cancel request_adapter.
  bool MaybeReportError(net::URLRequest* request) const;

  CronetURLRequestContextAdapter* context_;

  // Java object that owns this CronetURLRequestContextAdapter.
  base::android::ScopedJavaGlobalRef<jobject> owner_;

  const GURL initial_url_;
  const net::RequestPriority initial_priority_;
  std::string initial_method_;
  int load_flags_;
  net::HttpRequestHeaders initial_request_headers_;
  scoped_ptr<net::UploadDataStream> upload_;

  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  scoped_ptr<net::URLRequest> url_request_;

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestAdapter);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_
