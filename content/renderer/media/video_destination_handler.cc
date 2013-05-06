// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_destination_handler.h"

#include <string>

#include "base/logging.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using cricket::CaptureState;
using cricket::VideoFormat;
using webkit::ppapi::PPB_ImageData_Impl;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;

static const cricket::FourCC kEffectColorFormat = cricket::FOURCC_BGRA;

namespace content {

PpFrameWriter::PpFrameWriter()
    : started_(false) {}

PpFrameWriter::~PpFrameWriter() {}

CaptureState PpFrameWriter::Start(const VideoFormat& capture_format) {
  base::AutoLock auto_lock(lock_);
  if (started_) {
    LOG(ERROR) << "PpFrameWriter::Start - "
               << "Got a StartCapture when already started!";
    return cricket::CS_FAILED;
  }
  started_ = true;
  return cricket::CS_STARTING;
}

void PpFrameWriter::Stop() {
  base::AutoLock auto_lock(lock_);
  started_ = false;
  SignalStateChange(this, cricket::CS_STOPPED);
}

bool PpFrameWriter::IsRunning() {
  return started_;
}

bool PpFrameWriter::GetPreferredFourccs(std::vector<uint32>* fourccs) {
  if (!fourccs) {
    LOG(ERROR) << "PpFrameWriter::GetPreferredFourccs - "
               << "fourccs is NULL.";
    return false;
  }
  // The effects plugin output BGRA.
  fourccs->push_back(kEffectColorFormat);
  return true;
}

bool PpFrameWriter::GetBestCaptureFormat(const VideoFormat& desired,
                                         VideoFormat* best_format) {
  if (!best_format) {
    LOG(ERROR) << "PpFrameWriter::GetBestCaptureFormat - "
               << "best_format is NULL.";
    return false;
  }

  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = kEffectColorFormat;
  best_format->interval = desired.interval;
  return true;
}

bool PpFrameWriter::IsScreencast() const {
  return false;
}

void PpFrameWriter::PutFrame(PPB_ImageData_Impl* image_data,
                             int64 time_stamp_ns) {
  base::AutoLock auto_lock(lock_);
  // This assumes the handler of the SignalFrameCaptured won't call Start/Stop.
  // TODO(ronghuawu): Avoid the using of lock. One way is to post this call to
  // libjingle worker thread, which will require an extra copy of |image_data|.
  // However if pepper host can hand over the ownership of |image_data|
  // then we can avoid this extra copy.
  if (!started_) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - "
               << "Called when capturer is not started.";
    return;
  }
  ASSERT(image_data != NULL);
  webkit::ppapi::ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid()) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - "
               << "The image could not be mapped and is unusable.";
    return;
  }
  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  ASSERT(bitmap != NULL);

  cricket::CapturedFrame frame;
  frame.elapsed_time = 0;
  frame.time_stamp = time_stamp_ns;
  frame.pixel_height = 1;
  frame.pixel_width = 1;
  frame.width = bitmap->width();
  frame.height = bitmap->height();
  if (image_data->format() == PP_IMAGEDATAFORMAT_BGRA_PREMUL) {
    frame.fourcc = cricket::FOURCC_BGRA;
  } else {
    LOG(ERROR) << "PpFrameWriter::PutFrame - Got RGBA which is not supported.";
    ASSERT(false);
    return;
  }
  frame.data_size = bitmap->getSize();
  frame.data = bitmap->getPixels();

  // This signals to libJingle that a new VideoFrame is available.
  // libJingle have no assumptions on what thread this signal come from.
  SignalFrameCaptured(this, &frame);
}

// PpFrameWriterProxy is a helper class to make sure the user won't use
// PpFrameWriter after it is released (IOW its owner - WebMediaStreamTrack -
// is released).
class PpFrameWriterProxy : public FrameWriterInterface {
 public:
  PpFrameWriterProxy(VideoTrackInterface* track,
                     PpFrameWriter* writer)
      : track_(track),
        writer_(writer) {
    ASSERT(writer_ != NULL);
  }

  virtual ~PpFrameWriterProxy() {}

  virtual void PutFrame(webkit::ppapi::PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) OVERRIDE {
    writer_->PutFrame(image_data, time_stamp_ns);
  }

 private:
  scoped_refptr<VideoTrackInterface> track_;
  PpFrameWriter* writer_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameWriterProxy);
};

bool VideoDestinationHandler::Open(
    MediaStreamDependencyFactory* factory,
    MediaStreamRegistryInterface* registry,
    const std::string& url,
    FrameWriterInterface** frame_writer) {
  if (!factory) {
    factory = RenderThreadImpl::current()->GetMediaStreamDependencyFactory();
    ASSERT(factory != NULL);
  }
  WebKit::WebMediaStream stream;
  if (registry) {
    stream = registry->GetMediaStream(url);
  } else {
    stream =
        WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));
  }
  if (stream.isNull() || !stream.extraData()) {
    LOG(ERROR) << "VideoDestinationHandler::Open - invalid url: " << url;
    return false;
  }

  // Create a new native video track and add it to |stream|.
  std::string track_id = talk_base::ToString(talk_base::CreateRandomId64());
  PpFrameWriter* writer = new PpFrameWriter();
  if (!factory->AddNativeVideoMediaTrack(track_id, &stream, writer)) {
    delete writer;
    return false;
  }

  // Gets a handler to the native video track, which owns the |writer|.
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  DCHECK(extra_data);
  webrtc::MediaStreamInterface* native_stream = extra_data->stream();
  DCHECK(native_stream);
  VideoTrackVector video_tracks = native_stream->GetVideoTracks();
  // Currently one supports one video track per media stream.
  ASSERT(video_tracks.size() == 1);

  *frame_writer = new PpFrameWriterProxy(video_tracks[0].get(), writer);
  return true;
}

}  // namespace content

