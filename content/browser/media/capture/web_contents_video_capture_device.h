// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace content {

class ContentVideoCaptureDeviceCore;

// A virtualized VideoCaptureDevice that mirrors the displayed contents of a
// tab (accessed via its associated WebContents instance), producing a stream of
// video frames.
//
// An instance is created by providing a device_id.  The device_id contains
// information necessary for finding a WebContents instance.  From then on,
// WebContentsVideoCaptureDevice will capture from whatever render view is
// currently associated with that WebContents instance.  This allows the
// underlying render view to be swapped out (e.g., due to navigation or
// crashes/reloads), without any interruption in capturing.
//
// TODO(miu): In a soon upcoming change, the cross-site isolation migration of
// this code will be completed such that the main RenderFrameHost is tracked
// instead of the RenderViewHost.
class CONTENT_EXPORT WebContentsVideoCaptureDevice
    : public media::VideoCaptureDevice {
 public:
  // Create a WebContentsVideoCaptureDevice instance from the given
  // |device_id|.  Returns NULL if |device_id| is invalid.
  static media::VideoCaptureDevice* Create(const std::string& device_id);

  virtual ~WebContentsVideoCaptureDevice();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(const media::VideoCaptureParams& params,
                                scoped_ptr<Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

 private:
  WebContentsVideoCaptureDevice(
      int render_process_id, int main_render_frame_id);

  const scoped_ptr<ContentVideoCaptureDeviceCore> core_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDevice);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
