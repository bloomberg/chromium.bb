// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SIMPLE_URL_LOADER_H_
#define CONTENT_PUBLIC_COMMON_SIMPLE_URL_LOADER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace content {

struct ResourceRequest;
struct ResourceResponseHead;

namespace mojom {
class URLLoaderFactory;
}

// Creates and wraps a URLLoader, and runs it to completion. It's recommended
// that consumers use this class instead of URLLoader directly, due to the
// complexity of the API.
//
// Deleting a SimpleURLLoader before it completes cancels the requests and frees
// any resources it is using (including any partially downloaded files).
//
// Each SimpleURLLoader can only be used for a single request.
//
// TODO(mmenke): Support the following:
// * Save to (temp) file.
// * Consumer-provided methods to receive streaming (with backpressure).
// * Monitoring (And cancelling during) redirects.
// * Uploads (Fixed strings, files, data streams (with backpressure), chunked
// uploads). ResourceRequest may already have some support, but should make it
// simple.
// * Retrying.
class CONTENT_EXPORT SimpleURLLoader {
 public:
  // The maximum size DownloadToString will accept.
  const size_t kMaxBoundedStringDownloadSize = 1024 * 1024;

  // Callback used when downloading the response body as a std::string.
  // |response_body| is the body of the response, or nullptr on failure.
  using BodyAsStringCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body)>;

  static std::unique_ptr<SimpleURLLoader> Create();

  virtual ~SimpleURLLoader();

  // Starts a request for |resource_request| using |network_context|. The
  // SimpleURLLoader will accumulate all downloaded data in an in-memory string
  // of bounded size. If |max_body_size| is exceeded, the request will fail with
  // net::ERR_INSUFFICIENT_RESOURCES. |max_body_size| must be no greater than 1
  // MiB. For anything larger, it's recommended to either save to a temp file,
  // or consume the data as it is received.
  //
  // Whether the request succeeds or fails, or the body exceeds |max_body_size|,
  // |body_as_string_callback| will be invoked on completion.
  virtual void DownloadToString(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      BodyAsStringCallback body_as_string_callback,
      size_t max_body_size) = 0;

  // Same as DownloadToString, but downloads to a buffer of unbounded size,
  // potentially causing a crash if the amount of addressable memory is
  // exceeded. It's recommended consumers use DownloadToString instead.
  virtual void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      BodyAsStringCallback body_as_string_callback) = 0;

  // Sets whether partially received results are allowed. Defaults to false.
  // When true, if an error is received after reading the body starts or the max
  // allowed body size exceeded, the partial response body that was received
  // will be provided to the BodyAsStringCallback. The partial response body may
  // be an empty string.
  //
  // May only be called before the request is started.
  virtual void SetAllowPartialResults(bool allow_partial_results) = 0;

  // Sets whether bodies of non-2xx responses are returned. May only be called
  // before the request is started.
  //
  // When false, if a non-2xx result is received (Other than a redirect), the
  // request will fail with net::FAILED without waiting to read the response
  // body, though headers will be accessible through response_info().
  //
  // When true, non-2xx responses are treated no differently than other
  // responses, so their response body is returned just as with any other
  // response code, and when they complete, net_error() will return net::OK, if
  // no other problem occurs.
  //
  // Defaults to false.
  // TODO(mmenke): Consider adding a new error code for this.
  virtual void SetAllowHttpErrorResults(bool allow_http_error_results) = 0;

  // Returns the net::Error representing the final status of the request. May
  // only be called once the loader has informed the caller of completion.
  virtual int NetError() const = 0;

  // The ResourceResponseHead for the request. Will be nullptr if ResponseInfo
  // was never received. May only be called once the loader has informed the
  // caller of completion.
  virtual const ResourceResponseHead* ResponseInfo() const = 0;

 protected:
  SimpleURLLoader();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SIMPLE_URL_LOADER_H_
