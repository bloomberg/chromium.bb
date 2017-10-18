// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SIMPLE_URL_LOADER_H_
#define CONTENT_PUBLIC_COMMON_SIMPLE_URL_LOADER_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

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
// * Consumer-provided methods to receive streaming (with backpressure).
// * Monitoring (And cancelling during) redirects.
// * Uploads (Fixed strings, files, data streams (with backpressure), chunked
// uploads). ResourceRequest may already have some support, but should make it
// simple.
// * Maybe some sort of retry backoff or delay?  ServiceURLLoaderContext enables
// throttling for its URLFetchers.  Could additionally/alternatively support
// 503 + Retry-After.
class CONTENT_EXPORT SimpleURLLoader {
 public:
  // When a failed request should automatically be retried. These are intended
  // to be ORed together.
  enum RetryMode {
    RETRY_NEVER = 0x0,
    // Retries whenever the server returns a 5xx response code.
    RETRY_ON_5XX = 0x1,
    // Retries on net::ERR_NETWORK_CHANGED.
    RETRY_ON_NETWORK_CHANGE = 0x2,
  };

  // The maximum size DownloadToString will accept.
  const size_t kMaxBoundedStringDownloadSize = 1024 * 1024;

  // Callback used when downloading the response body as a std::string.
  // |response_body| is the body of the response, or nullptr on failure.
  using BodyAsStringCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body)>;

  // Callback used when download the response body to a file. On failure, |path|
  // will be empty.
  using DownloadToFileCompleteCallback =
      base::OnceCallback<void(const base::FilePath& path)>;

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
  // |body_as_string_callback| will be invoked on completion. Deleting the
  // SimpleURLLoader before the callback is invoked will return in cancelling
  // the request, and the callback will not be called.
  virtual void DownloadToString(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      BodyAsStringCallback body_as_string_callback,
      size_t max_body_size) = 0;

  // Same as DownloadToString, but downloads to a buffer of unbounded size,
  // potentially causing a crash if the amount of addressable memory is
  // exceeded. It's recommended consumers use one of the other download methods
  // instead (DownloadToString if the body is expected to be of reasonable
  // length, or DownloadToFile otherwise).
  virtual void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      BodyAsStringCallback body_as_string_callback) = 0;

  // SimpleURLLoader will download the entire response to a file at the
  // specified path. File I/O will happen on another sequence, so it's safe to
  // use this on any sequence.
  //
  // If there's a file, network, or http error, or the max limit
  // is exceeded, the file will be automatically destroyed before the callback
  // is invoked and en empty path passed to the callback, unless
  // SetAllowPartialResults() and/or SetAllowHttpErrorResults() were used to
  // indicate partial results are allowed.
  //
  // If the SimpleURLLoader is destroyed before it has invoked the callback, the
  // downloaded file will be deleted asynchronously and the callback will not be
  // invoked, regardless of other settings.
  virtual void DownloadToFile(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      const base::FilePath& file_path,
      int64_t max_body_size = std::numeric_limits<int64_t>::max()) = 0;

  // Same as DownloadToFile, but creates a temporary file instead of taking a
  // FilePath.
  virtual void DownloadToTempFile(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      int64_t max_body_size = std::numeric_limits<int64_t>::max()) = 0;

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

  // Sets the when to try and the max number of times to retry a request, if
  // any. |max_retries| is the number of times to retry the request, not
  // counting the initial request. |retry_mode| is a combination of one or more
  // RetryModes, indicating when the request should be retried. If it is
  // RETRY_NEVER, |max_retries| must be 0.
  //
  // By default, a request will not be retried.
  //
  // When a request is retried, the the request will start again using the
  // initial content::ResourceRequest, even if the request was redirected.
  //
  // Calling this multiple times will overwrite the values previously passed to
  // this method. May only be called before the request is started.
  virtual void SetRetryOptions(int max_retries, int retry_mode) = 0;

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
