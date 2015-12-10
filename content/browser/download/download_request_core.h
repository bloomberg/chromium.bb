// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_

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
class UrlDownloader;
struct DownloadCreateInfo;

// Forwards data to the download thread.
class CONTENT_EXPORT DownloadRequestCore
    : public base::SupportsWeakPtr<DownloadRequestCore> {
 public:
  // Size of the buffer used between the DownloadRequestCore and the
  // downstream receiver of its output.
  static const int kDownloadByteStreamSize;

  // started_cb will be called exactly once on the UI thread.
  // |id| should be invalid if the id should be automatically assigned.
  DownloadRequestCore(
      uint32 id,
      net::URLRequest* request,
      const DownloadUrlParameters::OnStartedCallback& started_cb,
      scoped_ptr<DownloadSaveInfo> save_info,
      base::WeakPtr<DownloadManagerImpl> download_manager);
  ~DownloadRequestCore();

  // Send the download creation information to the download thread.
  bool OnResponseStarted();

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size);

  bool OnReadCompleted(int bytes_read, bool* defer);

  void OnResponseCompleted(const net::URLRequestStatus& status);

  void PauseRequest();
  void ResumeRequest();

  std::string DebugString() const;

  void set_downloader(UrlDownloader* downloader) { downloader_ = downloader; }

 protected:
  net::URLRequest* request() const { return request_; }

 private:
  // Arrange for started_cb_ to be called on the UI thread with the
  // below values, nulling out started_cb_.  Should only be called
  // on the IO thread.
  void CallStartedCB(DownloadItem* item,
                     DownloadInterruptReason interrupt_reason);

  net::URLRequest* request_;
  uint32 download_id_;

  // This is read only on the IO thread, but may only
  // be called on the UI thread.
  DownloadUrlParameters::OnStartedCallback started_cb_;
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
  int64 bytes_read_;

  int pause_count_;
  bool was_deferred_;

  // For DCHECKing
  bool on_response_started_called_;

  UrlDownloader* downloader_;

  // DownloadManager passed in by the owner of DownloadRequestCore.
  base::WeakPtr<DownloadManagerImpl> download_manager_;

  static const int kReadBufSize = 32768;   // bytes
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_
