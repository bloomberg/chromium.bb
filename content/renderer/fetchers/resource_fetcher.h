// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper that simplifies the download of an HTTP object.  The interface is
// modeled after URLFetcher in /net/url_request/.
//
// The callback passed to the constructor will be called async after the
// resource is retrieved.

#ifndef CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_
#define CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

class GURL;

namespace WebKit {
class WebFrame;
class WebURLLoader;
struct WebURLError;
}

namespace content {

class CONTENT_EXPORT ResourceFetcher
    : NON_EXPORTED_BASE(public WebKit::WebURLLoaderClient) {
 public:
  // This will be called when the URL has been fetched, successfully or not.
  // If there is a failure, response and data will both be empty.  |response|
  // and |data| are both valid until the ResourceFetcher instance is destroyed.
  typedef base::Callback<void(const WebKit::WebURLResponse& response,
                              const std::string& data)> Callback;

  // A frame is needed to make requests.
  ResourceFetcher(
      const GURL& url, WebKit::WebFrame* frame,
      WebKit::WebURLRequest::TargetType target_type,
      const Callback& callback);

  // Deleting a ResourceFetcher will safely cancel the request if it has not yet
  // completed.
  virtual ~ResourceFetcher();

  // Sets a timeout for the request.  Request must not yet have completed
  // or timed out when called.
  void SetTimeout(base::TimeDelta timeout);

 private:
  // Start the actual download.
  void Start(const GURL& url, WebKit::WebFrame* frame,
             WebKit::WebURLRequest::TargetType target_type);

  void RunCallback(const WebKit::WebURLResponse& response,
                   const std::string& data);

  // Callback for timer that limits how long we wait for the server.  If this
  // timer fires and the request hasn't completed, we kill the request.
  void TimeoutFired();

  // WebURLLoaderClient methods:
  virtual void willSendRequest(
      WebKit::WebURLLoader* loader, WebKit::WebURLRequest& new_request,
      const WebKit::WebURLResponse& redirect_response);
  virtual void didSendData(
      WebKit::WebURLLoader* loader, unsigned long long bytes_sent,
      unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(
      WebKit::WebURLLoader* loader, const WebKit::WebURLResponse& response);
  virtual void didReceiveCachedMetadata(
      WebKit::WebURLLoader* loader, const char* data, int data_length);

  virtual void didReceiveData(
      WebKit::WebURLLoader* loader, const char* data, int data_length,
      int encoded_data_length);
  virtual void didFinishLoading(
      WebKit::WebURLLoader* loader, double finishTime);
  virtual void didFail(
      WebKit::WebURLLoader* loader, const WebKit::WebURLError& error);

  scoped_ptr<WebKit::WebURLLoader> loader_;

  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  WebKit::WebURLResponse response_;

  // Callback when we're done.
  Callback callback_;

  // Buffer to hold metadata from the cache.
  std::string metadata_;

  // Limit how long to wait for the server.
  base::OneShotTimer<ResourceFetcher> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_
