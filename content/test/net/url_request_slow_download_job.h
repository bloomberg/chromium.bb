// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates a slow download.  This used in a UI test to test the
// download manager.  Requests to |kUnknownSizeUrl| and |kKnownSizeUrl| start
// downloads that pause after the first N bytes, to be completed by sending a
// request to |kFinishDownloadUrl|.

#ifndef CONTENT_TEST_NET_URL_REQUEST_SLOW_DOWNLOAD_JOB_H_
#define CONTENT_TEST_NET_URL_REQUEST_SLOW_DOWNLOAD_JOB_H_

#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"

namespace content {

class URLRequestSlowDownloadJob : public net::URLRequestJob {
 public:
  // Test URLs.
  static const char kUnknownSizeUrl[];
  static const char kKnownSizeUrl[];
  static const char kFinishDownloadUrl[];
  static const char kErrorDownloadUrl[];

  // Download sizes.
  static const int kFirstDownloadSize;
  static const int kSecondDownloadSize;

  // Timer callback, used to check to see if we should finish our download and
  // send the second chunk.
  void CheckDoneStatus();

  // net::URLRequestJob methods
  virtual void Start() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     const std::string& scheme);

  // Returns the current number of URLRequestSlowDownloadJobs that have
  // not yet completed.
  static size_t NumberOutstandingRequests();

  // Adds the testing URLs to the net::URLRequestFilter.
  static void AddUrlHandler();

 private:
  URLRequestSlowDownloadJob(net::URLRequest* request,
                            net::NetworkDelegate* network_delegate);
  virtual ~URLRequestSlowDownloadJob();

  // Enum indicating where we are in the read after a call to
  // FillBufferHelper.
  enum ReadStatus {
    // The buffer was filled with data and may be returned.
    BUFFER_FILLED,

    // No data was added to the buffer because kFinishDownloadUrl has
    // not yet been seen and we've already returned the first chunk.
    REQUEST_BLOCKED,

    // No data was added to the buffer because we've already returned
    // all the data.
    REQUEST_COMPLETE
  };
  ReadStatus FillBufferHelper(
      net::IOBuffer* buf,
      int buf_size,
      int* bytes_written);

  void GetResponseInfoConst(net::HttpResponseInfo* info) const;

  // Mark all pending requests to be finished.  We keep track of pending
  // requests in |pending_requests_|.
  static void FinishPendingRequests();
  static void ErrorPendingRequests();
  typedef std::set<URLRequestSlowDownloadJob*> SlowJobsSet;
  static base::LazyInstance<SlowJobsSet>::Leaky pending_requests_;

  void StartAsync();

  void set_should_finish_download() { should_finish_download_ = true; }
  void set_should_error_download() { should_error_download_ = true; }

  int bytes_already_sent_;
  bool should_error_download_;
  bool should_finish_download_;
  scoped_refptr<net::IOBuffer> buffer_;
  int buffer_size_;

  base::WeakPtrFactory<URLRequestSlowDownloadJob> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_TEST_NET_URL_REQUEST_SLOW_DOWNLOAD_JOB_H_
