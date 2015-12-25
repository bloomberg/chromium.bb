// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/resource_handler.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {
class DownloadManagerImpl;
class ByteStreamReader;
class ByteStreamWriter;
class PowerSaveBlocker;
struct DownloadCreateInfo;

// This class encapsulates the core logic for reading data from a URLRequest and
// writing it into a ByteStream. It's common to both DownloadResourceHandler and
// UrlDownloader.
//
// Created, lives on and dies on the IO thread.
class CONTENT_EXPORT DownloadRequestCore
    : public base::SupportsWeakPtr<DownloadRequestCore> {
 public:
  // Size of the buffer used between the DownloadRequestCore and the
  // downstream receiver of its output.
  static const int kDownloadByteStreamSize;

  // |request| *must* outlive the DownloadRequestCore. |save_info| must be
  // valid.
  //
  // Invokes |on_ready_to_read_callback| if a previous call to OnReadCompleted()
  // resulted in |defer| being set to true, and DownloadRequestCore is now ready
  // to commence reading.
  DownloadRequestCore(net::URLRequest* request,
                      scoped_ptr<DownloadSaveInfo> save_info,
                      const base::Closure& on_ready_to_read_callback);
  ~DownloadRequestCore();

  // Should be called when the URLRequest::Delegate receives OnResponseStarted.
  // Constructs a DownloadCreateInfo and a ByteStreamReader that should be
  // passed into DownloadManagerImpl::StartDownload().
  //
  // Only populates the response derived fields of DownloadCreateInfo, with the
  // exception of |save_info|.
  void OnResponseStarted(scoped_ptr<DownloadCreateInfo>* info,
                         scoped_ptr<ByteStreamReader>* stream_reader);

  // Starts a read cycle. Creates a new IOBuffer which can be passed into
  // URLRequest::Read(). Call OnReadCompleted() when the Read operation
  // completes.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size);

  // Should be called when the Read() operation completes. |defer| will be set
  // to true if reading is to be suspended. In the latter case, once more data
  // can be read, invokes the |on_ready_to_read_callback|.
  bool OnReadCompleted(int bytes_read, bool* defer);

  // Called to signal that the response is complete. If the return value is
  // something other than DOWNLOAD_INTERRUPT_REASON_NONE, then the download
  // should be considered interrupted.
  //
  // It is expected that once this method is invoked, the DownloadRequestCore
  // object will be destroyed in short order without invoking any other methods
  // other than the destructor.
  DownloadInterruptReason OnResponseCompleted(
      const net::URLRequestStatus& status);

  // Called if the request should suspend reading. A subsequent
  // OnReadCompleted() will result in |defer| being set to true.
  //
  // Each PauseRequest() must be balanced with a call to ResumeRequest().
  void PauseRequest();

  // Balances a call to PauseRequest(). If no more pauses are outstanding and
  // the reader end of the ByteStream is ready to receive more data,
  // DownloadRequestCore will invoke the |on_ready_to_read_callback| to signal
  // to the caller that the read cycles should commence.
  void ResumeRequest();

  std::string DebugString() const;

 protected:
  net::URLRequest* request() const { return request_; }

 private:
  base::Closure on_ready_to_read_callback_;
  net::URLRequest* request_;
  scoped_ptr<DownloadSaveInfo> save_info_;

  // Data flow
  scoped_refptr<net::IOBuffer> read_buffer_;    // From URLRequest.
  scoped_ptr<ByteStreamWriter> stream_writer_;  // To rest of system.

  // Keeps the system from sleeping while this is alive. If the
  // system enters power saving mode while a request is alive, it can cause the
  // request to fail and the associated download will be interrupted.
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  // The following are used to collect stats.
  base::TimeTicks download_start_time_;
  base::TimeTicks last_read_time_;
  base::TimeTicks last_stream_pause_time_;
  base::TimeDelta total_pause_time_;
  size_t last_buffer_size_;
  int64_t bytes_read_;

  int pause_count_;
  bool was_deferred_;

  // Each successful OnWillRead will yield a buffer of this size.
  static const int kReadBufSize = 32768;   // bytes

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_
