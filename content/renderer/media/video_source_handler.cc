// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_source_handler.h"

#include <string>

#include "base/logging.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"
#include "third_party/libjingle/source/talk/media/base/videorenderer.h"

using cricket::VideoFrame;
using cricket::VideoRenderer;
using webrtc::VideoSourceInterface;

namespace content {

// PpFrameReceiver implements cricket::VideoRenderer so that it can be attached
// to native video track's video source to receive the captured frame.
// It can be attached to a FrameReaderInterface to output the received frame.
class PpFrameReceiver : public cricket::VideoRenderer {
 public:
  PpFrameReceiver() : reader_(NULL) {}
  virtual ~PpFrameReceiver() {}

  // Implements VideoRenderer.
  virtual bool SetSize(int width, int height, int reserved) OVERRIDE {
    return true;
  }
  virtual bool RenderFrame(const cricket::VideoFrame* frame) OVERRIDE {
    base::AutoLock auto_lock(lock_);
    if (reader_) {
      // Make a shallow copy of the frame as the |reader_| may need to queue it.
      // Both frames will share a single reference-counted frame buffer.
      reader_->GotFrame(frame->Copy());
    }
    return true;
  }

  void SetReader(FrameReaderInterface* reader) {
    base::AutoLock auto_lock(lock_);
    reader_ = reader;
  }

 private:
  FrameReaderInterface* reader_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameReceiver);
};

VideoSourceHandler::VideoSourceHandler(
    MediaStreamRegistryInterface* registry)
    : registry_(registry) {
}

VideoSourceHandler::~VideoSourceHandler() {}

bool VideoSourceHandler::Open(const std::string& url,
                              FrameReaderInterface* reader) {
  scoped_refptr<webrtc::VideoSourceInterface> source = GetFirstVideoSource(url);
  if (!source.get()) {
    return false;
  }
  PpFrameReceiver* receiver = new PpFrameReceiver();
  receiver->SetReader(reader);
  source->AddSink(receiver);
  reader_to_receiver_[reader] = receiver;
  return true;
}

bool VideoSourceHandler::Close(const std::string& url,
                               FrameReaderInterface* reader) {
  scoped_refptr<webrtc::VideoSourceInterface> source = GetFirstVideoSource(url);
  if (!source.get()) {
    LOG(ERROR) << "VideoSourceHandler::Close - Failed to get the video source "
               << "from MediaStream with url: " << url;
    return false;
  }
  PpFrameReceiver* receiver =
      static_cast<PpFrameReceiver*>(GetReceiver(reader));
  receiver->SetReader(NULL);
  source->RemoveSink(receiver);
  reader_to_receiver_.erase(reader);
  return true;
}

scoped_refptr<VideoSourceInterface> VideoSourceHandler::GetFirstVideoSource(
    const std::string& url) {
  scoped_refptr<webrtc::VideoSourceInterface> source;
  WebKit::WebMediaStream stream;
  if (registry_) {
    stream = registry_->GetMediaStream(url);
  } else {
    stream =
        WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));
  }
  if (stream.isNull() || !stream.extraData()) {
    LOG(ERROR) << "GetFirstVideoSource - invalid url: " << url;
    return source;
  }

  // Get the first video track from the stream.
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  if (!extra_data) {
    LOG(ERROR) << "GetFirstVideoSource - MediaStreamExtraData is NULL.";
    return source;
  }
  webrtc::MediaStreamInterface* native_stream = extra_data->stream();
  if (!native_stream) {
    LOG(ERROR) << "GetFirstVideoSource - native stream is NULL.";
    return source;
  }
  webrtc::VideoTrackVector native_video_tracks =
      native_stream->GetVideoTracks();
  if (native_video_tracks.empty()) {
    LOG(ERROR) << "GetFirstVideoSource - stream has no video track.";
    return source;
  }
  source = native_video_tracks[0]->GetSource();
  return source;
}

VideoRenderer* VideoSourceHandler::GetReceiver(
    FrameReaderInterface* reader) {
  std::map<FrameReaderInterface*, VideoRenderer*>::iterator it;
  it = reader_to_receiver_.find(reader);
  if (it == reader_to_receiver_.end()) {
    return NULL;
  }
  return it->second;
}

}  // namespace content

