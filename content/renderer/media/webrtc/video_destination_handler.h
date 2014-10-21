// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_DESTINATION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_DESTINATION_HANDLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/video_frame_pool.h"

namespace content {

class MediaStreamRegistryInterface;
class PPB_ImageData_Impl;

// VideoDestinationHandler is a glue class between the content MediaStream and
// the effects pepper plugin host.
class CONTENT_EXPORT VideoDestinationHandler {
 public:
  // FrameWriterCallback is used to forward frames from the pepper host.
  // It must be invoked on the main render thread.
  typedef base::Callback<
      void(const scoped_refptr<PPB_ImageData_Impl>& frame,
           int64 time_stamp_ns)> FrameWriterCallback;

  // Instantiates and adds a new video track to the MediaStream specified by
  // |url|. Returns a handler for delivering frames to the new video track as
  // |frame_writer|.
  // If |registry| is NULL the global blink::WebMediaStreamRegistry will be
  // used to look up the media stream.
  // Returns true on success and false on failure.
  static bool Open(MediaStreamRegistryInterface* registry,
                   const std::string& url,
                   FrameWriterCallback* frame_writer);

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDestinationHandler);
};

// PpFrameWriter implements MediaStreamVideoSource and can therefore provide
// video frames to MediaStreamVideoTracks.
class CONTENT_EXPORT PpFrameWriter
    : NON_EXPORTED_BASE(public MediaStreamVideoSource) {
 public:
  PpFrameWriter();
  virtual ~PpFrameWriter();

  // Returns a callback that can be used for delivering frames to this
  // MediaStreamSource implementation.
  VideoDestinationHandler::FrameWriterCallback GetFrameWriterCallback();

 protected:
  // MediaStreamVideoSource implementation.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;
  void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const VideoCaptureDeliverFrameCB& frame_callback) override;
  void StopSourceImpl() override;

 private:
  class FrameWriterDelegate;
  scoped_refptr<FrameWriterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameWriter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_DESTINATION_HANDLER_H_
