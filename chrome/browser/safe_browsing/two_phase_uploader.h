// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TWO_PHASE_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TWO_PHASE_UPLOADER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace base {
class TaskRunner;
}
namespace net {
class URLRequestContextGetter;
}

class TwoPhaseUploaderFactory;

// Implements the Google two-phase resumable upload protocol.
// Protocol documentation:
// https://developers.google.com/storage/docs/developer-guide#resumable
// Note: This doc is for the Cloud Storage API which specifies the POST body
// must be empty, however the safebrowsing use of the two-phase protocol
// supports sending metadata in the POST request body. We also do not need the
// api-version and authorization headers.
// TODO(mattm): support retry / resume.
// Lives on the UI thread.
class TwoPhaseUploader {
 public:
  enum State {
    STATE_NONE,
    UPLOAD_METADATA,
    UPLOAD_FILE,
    STATE_SUCCESS,
  };
  using ProgressCallback = base::Callback<void(int64_t sent, int64_t total)>;
  using FinishCallback = base::Callback<void(State state,
                                             int net_error,
                                             int response_code,
                                             const std::string& response_data)>;

  virtual ~TwoPhaseUploader() {}

  // Create the uploader.  The Start method must be called to begin the upload.
  // Network processing will use |url_request_context_getter|.
  // The uploaded |file_path| will be read on |file_task_runner|.
  // The first phase request will be sent to |base_url|, with |metadata|
  // included.
  // |progress_callback| will be called periodically as the second phase
  // progresses, if it is non-null.
  // On success |finish_callback| will be called with state = STATE_SUCCESS and
  // the server response in response_data. On failure, state will specify
  // which step the failure occurred in, and net_error, response_code, and
  // response_data will specify information about the error. |finish_callback|
  // will not be called if the upload is cancelled by destructing the
  // TwoPhaseUploader object before completion.
  static std::unique_ptr<TwoPhaseUploader> Create(
      net::URLRequestContextGetter* url_request_context_getter,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      const ProgressCallback& progress_callback,
      const FinishCallback& finish_callback);

  // Makes the passed |factory| the factory used to instantiate
  // a TwoPhaseUploader. Useful for tests.
  static void RegisterFactory(TwoPhaseUploaderFactory* factory) {
    factory_ = factory;
  }

  // Begins the upload process.
  virtual void Start() = 0;

 private:
  // The factory that controls the creation of SafeBrowsingProtocolManager.
  // This is used by tests.
  static TwoPhaseUploaderFactory* factory_;
};

class TwoPhaseUploaderFactory {
 public:
  virtual ~TwoPhaseUploaderFactory() {}

  virtual std::unique_ptr<TwoPhaseUploader> CreateTwoPhaseUploader(
      net::URLRequestContextGetter* url_request_context_getter,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      const TwoPhaseUploader::ProgressCallback& progress_callback,
      const TwoPhaseUploader::FinishCallback& finish_callback) = 0;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_TWO_PHASE_UPLOADER_H_
