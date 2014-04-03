// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/video_destination_handler.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/video/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "url/gurl.h"

namespace content {

PpFrameWriter::PpFrameWriter(MediaStreamDependencyFactory* factory)
    : MediaStreamVideoSource(factory), first_frame_received_(false) {
  DVLOG(3) << "PpFrameWriter ctor";
}

PpFrameWriter::~PpFrameWriter() {
  DVLOG(3) << "PpFrameWriter dtor";
}

void PpFrameWriter::GetCurrentSupportedFormats(int max_requested_width,
                                               int max_requested_height) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "PpFrameWriter::GetCurrentSupportedFormats()";
  if (format_.IsValid()) {
    media::VideoCaptureFormats formats;
    formats.push_back(format_);
    OnSupportedFormats(formats);
  }
}

void PpFrameWriter::StartSourceImpl(
    const media::VideoCaptureParams& params) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "PpFrameWriter::StartSourceImpl()";
  OnStartDone(true);
}

void PpFrameWriter::StopSourceImpl() {
  DCHECK(CalledOnValidThread());
}

void PpFrameWriter::PutFrame(PPB_ImageData_Impl* image_data,
                             int64 time_stamp_ns) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "PpFrameWriter::PutFrame()";

  if (!image_data) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - Called with NULL image_data.";
    return;
  }
  ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid()) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - "
               << "The image could not be mapped and is unusable.";
    return;
  }
  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  if (!bitmap) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - "
               << "The image_data's mapped bitmap is NULL.";
    return;
  }

  const gfx::Size frame_size(bitmap->width(), bitmap->height());

  if (!first_frame_received_) {
    first_frame_received_ = true;
    format_ = media::VideoCaptureFormat(
        frame_size,
        MediaStreamVideoSource::kDefaultFrameRate,
        media::PIXEL_FORMAT_I420);
    if (state() == MediaStreamVideoSource::RETRIEVING_CAPABILITIES) {
      media::VideoCaptureFormats formats;
      formats.push_back(format_);
      OnSupportedFormats(formats);
    }
  }

  if (state() != MediaStreamVideoSource::STARTED)
    return;

  const base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
      time_stamp_ns / talk_base::kNumNanosecsPerMillisec);

  scoped_refptr<media::VideoFrame> new_frame =
      frame_pool_.CreateFrame(media::VideoFrame::I420, frame_size,
                              gfx::Rect(frame_size), frame_size, timestamp);

  libyuv::BGRAToI420(reinterpret_cast<uint8*>(bitmap->getPixels()),
                     bitmap->rowBytes(),
                     new_frame->data(media::VideoFrame::kYPlane),
                     new_frame->stride(media::VideoFrame::kYPlane),
                     new_frame->data(media::VideoFrame::kUPlane),
                     new_frame->stride(media::VideoFrame::kUPlane),
                     new_frame->data(media::VideoFrame::kVPlane),
                     new_frame->stride(media::VideoFrame::kVPlane),
                     frame_size.width(), frame_size.height());

  DeliverVideoFrame(new_frame);
}

// PpFrameWriterProxy is a helper class to make sure the user won't use
// PpFrameWriter after it is released (IOW its owner - WebMediaStreamSource -
// is released).
class PpFrameWriterProxy : public FrameWriterInterface {
 public:
  explicit PpFrameWriterProxy(const base::WeakPtr<PpFrameWriter>& writer)
      : writer_(writer) {
    DCHECK(writer_ != NULL);
  }

  virtual ~PpFrameWriterProxy() {}

  virtual void PutFrame(PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) OVERRIDE {
    writer_->PutFrame(image_data, time_stamp_ns);
  }

 private:
  base::WeakPtr<PpFrameWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameWriterProxy);
};

bool VideoDestinationHandler::Open(
    MediaStreamDependencyFactory* factory,
    MediaStreamRegistryInterface* registry,
    const std::string& url,
    FrameWriterInterface** frame_writer) {
  DVLOG(3) << "VideoDestinationHandler::Open";
  if (!factory) {
    factory = RenderThreadImpl::current()->GetMediaStreamDependencyFactory();
    DCHECK(factory != NULL);
  }
  blink::WebMediaStream stream;
  if (registry) {
    stream = registry->GetMediaStream(url);
  } else {
    stream =
        blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));
  }
  if (stream.isNull()) {
    LOG(ERROR) << "VideoDestinationHandler::Open - invalid url: " << url;
    return false;
  }

  // Create a new native video track and add it to |stream|.
  std::string track_id;
  // According to spec, a media stream source's id should be unique per
  // application. There's no easy way to strictly achieve that. The id
  // generated with this method should be unique for most of the cases but
  // theoretically it's possible we can get an id that's duplicated with the
  // existing sources.
  base::Base64Encode(base::RandBytesAsString(64), &track_id);
  PpFrameWriter* writer = new PpFrameWriter(factory);

  // Create a new webkit video track.
  blink::WebMediaStreamSource webkit_source;
  blink::WebMediaStreamSource::Type type =
      blink::WebMediaStreamSource::TypeVideo;
  blink::WebString webkit_track_id = base::UTF8ToUTF16(track_id);
  webkit_source.initialize(webkit_track_id, type, webkit_track_id);
  webkit_source.setExtraData(writer);

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  bool track_enabled = true;

  stream.addTrack(MediaStreamVideoTrack::CreateVideoTrack(
      writer, constraints, MediaStreamVideoSource::ConstraintsCallback(),
      track_enabled, factory));

  *frame_writer = new PpFrameWriterProxy(writer->AsWeakPtr());
  return true;
}

}  // namespace content

