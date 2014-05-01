// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_MEDIA_ACCESS_PERMISSION_REQUEST_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_MEDIA_ACCESS_PERMISSION_REQUEST_H

#include "android_webview/native/permission/aw_permission_request_delegate.h"
#include "base/callback.h"
#include "content/public/common/media_stream_request.h"

namespace android_webview {

// The AwPermissionRequestDelegate implementation for media access permission
// request.
class MediaAccessPermissionRequest : public AwPermissionRequestDelegate {
 public:
  MediaAccessPermissionRequest(const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback);
  virtual ~MediaAccessPermissionRequest();

  // AwPermissionRequestDelegate implementation.
  virtual const GURL& GetOrigin() OVERRIDE;
  virtual int64 GetResources() OVERRIDE;
  virtual void NotifyRequestResult(bool allowed) OVERRIDE;

 private:
  friend class TestMediaAccessPermissionRequest;

  const content::MediaStreamRequest request_;
  const content::MediaResponseCallback callback_;

  // For test only.
  content::MediaStreamDevices audio_test_devices_;
  content::MediaStreamDevices video_test_devices_;

  DISALLOW_COPY_AND_ASSIGN(MediaAccessPermissionRequest);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_MEDIA_ACCESS_PERMISSION_REQUEST_H
