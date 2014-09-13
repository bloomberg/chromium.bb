// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_WEB_URL_LOADER_CLIENT_IMPL_H_
#define CONTENT_RENDERER_FETCHERS_WEB_URL_LOADER_CLIENT_IMPL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace blink {
class WebURLLoader;
struct WebURLError;
}

namespace content {

class WebURLLoaderClientImpl : public blink::WebURLLoaderClient {
 protected:
  enum LoadStatus {
    LOADING,
    LOAD_FAILED,
    LOAD_SUCCEEDED,
  };

  WebURLLoaderClientImpl();
  virtual ~WebURLLoaderClientImpl();

  virtual void Cancel();

  bool completed() const { return completed_; }
  const std::string& data() const { return data_; }
  const blink::WebURLResponse& response() const { return response_; }
  const std::string& metadata() const { return metadata_; }
  LoadStatus status() const { return status_; }

  virtual void OnLoadComplete() = 0;

 private:
  void OnLoadCompleteInternal(LoadStatus);

  // WebWebURLLoaderClientImpl methods:
  virtual void didReceiveResponse(
      blink::WebURLLoader* loader, const blink::WebURLResponse& response);
  virtual void didReceiveCachedMetadata(
      blink::WebURLLoader* loader, const char* data, int data_length);
  virtual void didReceiveData(
      blink::WebURLLoader* loader,
      const char* data,
      int data_length,
      int encoded_data_length);
  virtual void didFinishLoading(
      blink::WebURLLoader* loader,
      double finishTime,
      int64_t total_encoded_data_length);
  virtual void didFail(
      blink::WebURLLoader* loader, const blink::WebURLError& error);

 private:
  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  blink::WebURLResponse response_;

  // Buffer to hold metadata from the cache.
  std::string metadata_;

  LoadStatus status_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_URL_LOADER_CLIENT_H_
