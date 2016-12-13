// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_IO_THREAD_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_IO_THREAD_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

namespace android_webview {

class AwWebResourceResponse;

// This class provides a means of calling Java methods on an instance that has
// a 1:1 relationship with a WebContents instance directly from the IO thread.
//
// Specifically this is used to associate URLRequests with the WebContents that
// the URLRequest is made for.
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
  // Corresponds to WebSettings cache mode constants.
  enum CacheMode {
    LOAD_DEFAULT = -1,
    LOAD_NORMAL = 0,
    LOAD_CACHE_ELSE_NETWORK = 1,
    LOAD_NO_CACHE = 2,
    LOAD_CACHE_ONLY = 3,
  };

  virtual ~AwContentsIoThreadClient() {}

  // Returns whether this is a new pop up that is still waiting for association
  // with the java counter part.
  virtual bool PendingAssociation() const = 0;

  // Retrieve CacheMode setting value of this AwContents.
  // This method is called on the IO thread only.
  virtual CacheMode GetCacheMode() const = 0;

  // This will attempt to fetch the AwContentsIoThreadClient for the given
  // |render_process_id|, |render_frame_id| pair.
  // This method can be called from any thread.
  // An empty scoped_ptr is a valid return value.
  static std::unique_ptr<AwContentsIoThreadClient> FromID(int render_process_id,
                                                          int render_frame_id);

  // Returns the global thread client for service worker related callbacks.
  // An empty scoped_ptr is a valid return value.
  static std::unique_ptr<AwContentsIoThreadClient>
  GetServiceWorkerIoThreadClient();

  // Called on the IO thread when a subframe is created.
  static void SubFrameCreated(int render_process_id,
                              int parent_render_frame_id,
                              int child_render_frame_id);

  // This method is called on the IO thread only.
  typedef base::Callback<void(std::unique_ptr<AwWebResourceResponse>)>
      ShouldInterceptRequestResultCallback;
  virtual void ShouldInterceptRequestAsync(
      const net::URLRequest* request,
      const ShouldInterceptRequestResultCallback callback) = 0;

  // Retrieve the AllowContentAccess setting value of this AwContents.
  // This method is called on the IO thread only.
  virtual bool ShouldBlockContentUrls() const = 0;

  // Retrieve the AllowFileAccess setting value of this AwContents.
  // This method is called on the IO thread only.
  virtual bool ShouldBlockFileUrls() const = 0;

  // Retrieve the BlockNetworkLoads setting value of this AwContents.
  // This method is called on the IO thread only.
  virtual bool ShouldBlockNetworkLoads() const = 0;

  // Retrieve the AcceptThirdPartyCookies setting value of this AwContents.
  virtual bool ShouldAcceptThirdPartyCookies() const = 0;

  // Called when a resource loading error has occured (e.g. an I/O error,
  // host name lookup failure etc.)
  virtual void OnReceivedError(const net::URLRequest* request) = 0;

  // Called when a response from the server is received with status code >= 400.
  virtual void OnReceivedHttpError(
      const net::URLRequest* request,
      const net::HttpResponseHeaders* response_headers) = 0;
};

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_IO_THREAD_CLIENT_H_
