// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_event_log_uploader.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace content {

WebRtcEventLogUploaderImpl::WebRtcEventLogUploaderImpl(
    const base::FilePath& path,
    WebRtcEventLogUploaderObserver* observer) {
  DCHECK(observer);

  // TODO(eladalon): Provide an actual implementation; really upload the file.
  // https://crbug.com/775415

  // If the upload was successful, the file is no longer needed.
  // If the upload failed, we don't want to retry, because we run the risk of
  // uploading significant amounts of data once again, only for the upload to
  // fail again after (as an example) wasting 50MBs of upload bandwidth.
  const bool deletion_successful = base::DeleteFile(path, /*recursive=*/false);
  if (!deletion_successful) {
    // This is a somewhat serious (though unlikely) error, because now we'll try
    // to upload this file again next time Chrome launches.
    LOG(ERROR) << "Could not delete pending log file.";
  }

  // TODO(eladalon): Provide actual success/failure of upload.
  // https://crbug.com/775415
  observer->OnWebRtcEventLogUploadComplete(path, true);
}

std::unique_ptr<WebRtcEventLogUploader>
WebRtcEventLogUploaderImpl::Factory::Create(
    const base::FilePath& log_file,
    WebRtcEventLogUploaderObserver* observer) {
  return std::make_unique<WebRtcEventLogUploaderImpl>(log_file, observer);
}

}  // namespace content
