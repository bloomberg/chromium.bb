// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_

#include <memory>

#include "base/files/file_path.h"

namespace content {

// A class implementing this interace can register for notification of an
// upload's eventual result (success/failure).
class WebRtcEventLogUploaderObserver {
 public:
  virtual void OnWebRtcEventLogUploadComplete(const base::FilePath& file_path,
                                              bool upload_successful) = 0;

 protected:
  virtual ~WebRtcEventLogUploaderObserver() = default;
};

// A sublcass of this interface would take ownership of a file, and either
// upload it to a remote server (actual implementation), or pretend to do
// so (in unit tests). It will typically take on an observer of type
// WebRtcEventLogUploaderObserver, and inform it of the success or failure
// of the upload.
class WebRtcEventLogUploader {
 public:
  virtual ~WebRtcEventLogUploader() = default;

  // Since we'll need more than one instance of the abstract
  // WebRtcEventLogUploader, we'll need an abstract factory for it.
  class Factory {
   public:
    virtual ~Factory() = default;

    // Creates uploaders. The observer is passed to each call of Create,
    // rather than be memorized by the factory's constructor, because factories
    // created by unit tests have no visibility into the real implementation's
    // observer (WebRtcRemoteEventLogManager).
    virtual std::unique_ptr<WebRtcEventLogUploader> Create(
        const base::FilePath& log_file,
        WebRtcEventLogUploaderObserver* observer) = 0;
  };
};

class WebRtcEventLogUploaderImpl : public WebRtcEventLogUploader {
 public:
  WebRtcEventLogUploaderImpl(const base::FilePath& path,
                             WebRtcEventLogUploaderObserver* observer);
  ~WebRtcEventLogUploaderImpl() override = default;

  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    ~Factory() override = default;

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const base::FilePath& log_file,
        WebRtcEventLogUploaderObserver* observer) override;
  };
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_
