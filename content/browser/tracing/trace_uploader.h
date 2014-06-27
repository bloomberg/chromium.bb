// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_UPLOADER_H_
#define CONTENT_BROWSER_TRACING_TRACE_UPLOADER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace content {

namespace {

// Allow up to 10MB for trace upload
const int kMaxUploadBytes = 10000000;

}  // namespace

// TraceUploader uploads traces.
class TraceUploader : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(bool, const std::string&, const std::string&)>
      UploadDoneCallback;
  typedef base::Callback<void(int64, int64)> UploadProgressCallback;

  TraceUploader(const std::string& product,
                const std::string& version,
                const std::string& upload_url,
                net::URLRequestContextGetter* request_context);
  virtual ~TraceUploader();

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

  // Compresses and uploads the given file contents.
  void DoUpload(const std::string& file_contents,
                UploadProgressCallback progress_callback,
                UploadDoneCallback done_callback);

 private:
  // Sets up a multipart body to be uploaded. The body is produced according
  // to RFC 2046.
  void SetupMultipart(const std::string& trace_filename,
                      const std::string& trace_contents,
                      std::string* post_data);
  void AddTraceFile(const std::string& trace_filename,
                    const std::string& trace_contents,
                    std::string* post_data);
  // Compresses the input and returns whether compression was successful.
  bool Compress(std::string input,
                int max_compressed_bytes,
                char* compressed_contents,
                int* compressed_bytes);
  void CreateAndStartURLFetcher(const std::string& post_data);
  void OnUploadError(std::string error_message);

  std::string product_;
  std::string version_;

  std::string upload_url_;

  scoped_ptr<net::URLFetcher> url_fetcher_;
  UploadProgressCallback progress_callback_;
  UploadDoneCallback done_callback_;

  net::URLRequestContextGetter* request_context_;

  DISALLOW_COPY_AND_ASSIGN(TraceUploader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_UPLOADER_H_
