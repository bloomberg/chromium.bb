// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TWO_PHASE_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TWO_PHASE_UPLOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/threading/non_thread_safe.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class TaskRunner;
}
namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// Implements the Google two-phase resumable upload protocol.
// Protocol documentation:
// https://developers.google.com/storage/docs/developer-guide#resumable
// Note: This doc is for the Cloud Storage API which specifies the POST body
// must be empty, however the safebrowsing use of the two-phase protocol
// supports sending metadata in the POST request body. We also do not need the
// api-version and authorization headers.
// TODO(mattm): support retry / resume.
class TwoPhaseUploader : public net::URLFetcherDelegate,
                         public base::NonThreadSafe {
 public:
  enum State {
    STATE_NONE,
    UPLOAD_METADATA,
    UPLOAD_FILE,
    STATE_SUCCESS,
  };
  typedef base::Callback<void(int64 sent, int64 total)> ProgressCallback;
  typedef base::Callback<void(State state,
                              int net_error,
                              int response_code,
                              const std::string& response_data)> FinishCallback;

  // Create the uploader.  The Start method must be called to begin the upload.
  // Network processing will use |url_request_context_getter|.
  // The uploaded |file_path| will be read on |file_task_runner|.
  // The first phase request will be sent to |base_url|, with |metadata|
  // included.
  // |progress_callback| will be called periodically as the second phase
  // progresses.
  // On success |finish_callback| will be called with state = STATE_SUCCESS and
  // the server response in response_data. On failure, state will specify
  // which step the failure occurred in, and net_error, response_code, and
  // response_data will specify information about the error. |finish_callback|
  // will not be called if the upload is cancelled by destructing the
  // TwoPhaseUploader object before completion.
  TwoPhaseUploader(
      net::URLRequestContextGetter* url_request_context_getter,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      const ProgressCallback& progress_callback,
      const FinishCallback& finish_callback);
  virtual ~TwoPhaseUploader();

  // Begins the upload process.
  void Start();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

 private:
  void UploadMetadata();
  void UploadFile();
  void Finish(int net_error, int response_code, const std::string& response);

  State state_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  GURL base_url_;
  GURL upload_url_;
  std::string metadata_;
  const base::FilePath file_path_;
  ProgressCallback progress_callback_;
  FinishCallback finish_callback_;

  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TwoPhaseUploader);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_TWO_PHASE_UPLOADER_H_
