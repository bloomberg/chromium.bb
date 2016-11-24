// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderClient.h"

namespace blink {
class WebAssociatedURLLoader;
struct WebURLError;
class WebURLResponse;
}

namespace nacl {

// Downloads a file and writes the contents to a specified file open for
// writing.
class FileDownloader : public blink::WebAssociatedURLLoaderClient {
 public:
  enum Status {
    SUCCESS,
    ACCESS_DENIED,
    FAILED
  };

  // Provides the FileDownloader status and the HTTP status code.
  typedef base::Callback<void(Status, base::File, int)> StatusCallback;

  // Provides the bytes received so far, and the total bytes expected to be
  // received.
  typedef base::Callback<void(int64_t, int64_t)> ProgressCallback;

  FileDownloader(std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
                 base::File file,
                 StatusCallback status_cb,
                 ProgressCallback progress_cb);

  ~FileDownloader() override;

  void Load(const blink::WebURLRequest& request);

 private:
  // WebAssociatedURLLoaderClient implementation.
  void didReceiveResponse(const blink::WebURLResponse& response) override;
  void didReceiveData(const char* data, int data_length) override;
  void didFinishLoading(double finish_time) override;
  void didFail(const blink::WebURLError& error) override;

  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader_;
  base::File file_;
  StatusCallback status_cb_;
  ProgressCallback progress_cb_;
  int http_status_code_;
  int64_t total_bytes_received_;
  int64_t total_bytes_to_be_received_;
  Status status_;
};

}  // namespace nacl
