// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_DESTINATION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_DESTINATION_HANDLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"

namespace webkit {
namespace ppapi {
class PPB_ImageData_Impl;
}
}

namespace content {

class MediaStreamDependencyFactory;
class MediaStreamRegistryInterface;

// Interface used by the effects pepper plugin to output the processed frame
// to the video track.
class CONTENT_EXPORT FrameWriterInterface {
 public:
  // The ownership of the |image_data| deosn't transfer. So the implementation
  // of this interface should make a copy of the |image_data| before return.
  virtual void PutFrame(webkit::ppapi::PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) = 0;
  virtual ~FrameWriterInterface() {}
};

// PpFrameWriter implements cricket::VideoCapturer so that it can be used in
// the native video track's video source. It also implements
// FrameWriterInterface, which will be used by the effects pepper plugin to
// inject the processed frame.
class CONTENT_EXPORT PpFrameWriter
    : public NON_EXPORTED_BASE(cricket::VideoCapturer),
      public FrameWriterInterface {
 public:
  PpFrameWriter();
  virtual ~PpFrameWriter();

  // cricket::VideoCapturer implementation.
  // These methods are accessed from a libJingle worker thread.
  virtual cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsRunning() OVERRIDE;
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs) OVERRIDE;
  virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                    cricket::VideoFormat* best_format) OVERRIDE;
  virtual bool IsScreencast() const OVERRIDE;

  // FrameWriterInterface implementation.
  // This method will be called by the Pepper host from render thread.
  virtual void PutFrame(webkit::ppapi::PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) OVERRIDE;

 private:
  bool started_;
  // |lock_| is used to protect |started_| which will be accessed from different
  // threads - libjingle worker thread and render thread.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameWriter);
};

// VideoDestinationHandler is a glue class between the webrtc MediaStream and
// the effects pepper plugin host.
class CONTENT_EXPORT VideoDestinationHandler {
 public:
  // Instantiates and adds a new video track to the MediaStream specified by
  // |url|. Returns a handler for delivering frames to the new video track as
  // |frame_writer|.
  // If |factory| is NULL the MediaStreamDependencyFactory owned by
  // RenderThreadImpl::current() will be used.
  // If |registry| is NULL the global WebKit::WebMediaStreamRegistry will be
  // used to look up the media stream.
  // The caller of the function takes the ownership of |frame_writer|.
  // Returns true on success and false on failure.
  static bool Open(MediaStreamDependencyFactory* factory,
                   MediaStreamRegistryInterface* registry,
                   const std::string& url,
                   FrameWriterInterface** frame_writer);

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDestinationHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_DESTINATION_HANDLER_H_

