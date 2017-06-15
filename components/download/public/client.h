// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_CLIENT_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_CLIENT_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace download {

// The Client interface required by any feature that wants to start a download
// through the DownloadService.  Should be registered immediately at startup
// when the DownloadService is created (see the factory).
class Client {
 public:
  // Used by OnDownloadStarted to determine whether or not the DownloadService
  // should continue downloading the file or abort the attempt.
  enum class ShouldDownload {
    CONTINUE,
    ABORT,
  };

  // Used by OnDownloadFailed to determine the reason of the abort.
  enum class FailureReason {
    // Used when the download has been aborted after reaching a threshold where
    // we decide it is not worth attempting to start again.  This could be
    // either due to a specific number of failed retry attempts or a specific
    // number of wasted bytes due to the download restarting.
    NETWORK,

    // Used when the download was not completed before the
    // DownloadParams::cancel_after timeout.
    TIMEDOUT,

    // Used when the download was cancelled by the Client.
    CANCELLED,

    // Used when the download was aborted by the Client in response to the
    // download starting (see OnDownloadStarted()).
    ABORTED,

    // Used when the failure reason is unknown.  This generally means that we
    // detect that the download failed during a restart, but aren't sure exactly
    // what triggered the failure before shutdown.
    UNKNOWN,
  };

  virtual ~Client() = default;

  // Called when the DownloadService is initialized and ready to be interacted
  // with.  |outstanding_download_guids| is a list of all downloads the
  // DownloadService is aware of that are associated with this Client.
  virtual void OnServiceInitialized(
      const std::vector<std::string>& outstanding_download_guids) = 0;

  // Return whether or not the download should be aborted (potentially in
  // response to |headers|).  The download will be downloading at the time this
  // call is made.
  virtual ShouldDownload OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) = 0;

  // Will be called when there is an update to the current progress state of the
  // underlying download.  Note that |bytes_downloaded| may go backwards if the
  // download had to be started over from the beginning due to an interruption.
  // This will be called frequently if the download is actively downloading,
  // with byte updates coming in as they are processed by the internal download
  // driver.
  virtual void OnDownloadUpdated(const std::string& guid,
                                 uint64_t bytes_downloaded) = 0;

  // Called when a download failed.  Check FailureReason for a list of possible
  // reasons why this failure occurred.  Note that this will also be called for
  // cancelled downloads.
  virtual void OnDownloadFailed(const std::string& guid,
                                FailureReason reason) = 0;

  // Called when a download has been successfully completed.  After this call
  // the download entry will be purged from the database.  The file will be
  // automatically removed if it is not renamed or deleted after a window of
  // time (12 hours, but finch configurable).  The timeout is meant to be a
  // failsafe to ensure that we clean up properly.
  // TODO(dtrainor): Investigate alternate output formats.
  // TODO(dtrainor): Point to finch configurable timeout when it is added.
  virtual void OnDownloadSucceeded(const std::string& guid,
                                   const base::FilePath& path,
                                   uint64_t size) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_CLIENT_H_
