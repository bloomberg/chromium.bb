// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

namespace blink {
struct WebURLError;
class WebURLLoader;
class WebURLResponse;
}

namespace nacl {

// Downloads a file and writes the contents to a specified file open for
// writing.
class FileDownloader : public blink::WebURLLoaderClient {
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

  FileDownloader(scoped_ptr<blink::WebURLLoader> url_loader,
                 base::File file,
                 StatusCallback status_cb,
                 ProgressCallback progress_cb);

  virtual ~FileDownloader();

  void Load(const blink::WebURLRequest& request);

 private:
  // WebURLLoaderClient implementation.
  virtual void didReceiveResponse(blink::WebURLLoader* loader,
                                  const blink::WebURLResponse& response);
  virtual void didReceiveData(blink::WebURLLoader* loader,
                              const char* data,
                              int data_length,
                              int encoded_data_length);
  virtual void didFinishLoading(blink::WebURLLoader* loader,
                                double finish_time,
                                int64_t total_encoded_data_length);
  virtual void didFail(blink::WebURLLoader* loader,
                       const blink::WebURLError& error);

  scoped_ptr<blink::WebURLLoader> url_loader_;
  base::File file_;
  StatusCallback status_cb_;
  ProgressCallback progress_cb_;
  int http_status_code_;
  int64_t total_bytes_received_;
  int64_t total_bytes_to_be_received_;
  Status status_;
};

}  // namespace nacl
