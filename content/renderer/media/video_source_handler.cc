// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_source_handler.h"

#include <string>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

namespace content {

// PpFrameReceiver implements MediaStreamVideoSink so that it can be attached
// to video track to receive the captured frame.
// It can be attached to a FrameReaderInterface to output the received frame.
class PpFrameReceiver : public MediaStreamVideoSink {
 public:
  PpFrameReceiver() : reader_(NULL) {}
  virtual ~PpFrameReceiver() {}

  // Implements MediaStreamVideoSink.
  virtual void OnVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE {
    base::AutoLock auto_lock(lock_);
    if (reader_) {
      reader_->GotFrame(frame);
    }
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

VideoSourceHandler::~VideoSourceHandler() {
  for (SourceInfoMap::iterator it = reader_to_receiver_.begin();
       it != reader_to_receiver_.end();
       ++it) {
    delete it->second;
  }
}

bool VideoSourceHandler::Open(const std::string& url,
                              FrameReaderInterface* reader) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const blink::WebMediaStreamTrack& track = GetFirstVideoTrack(url);
  if (track.isNull()) {
    return false;
  }
  reader_to_receiver_[reader] = new SourceInfo(track, reader);
  return true;
}

bool VideoSourceHandler::Close(FrameReaderInterface* reader) {
  DCHECK(thread_checker_. CalledOnValidThread());
  SourceInfoMap::iterator it = reader_to_receiver_.find(reader);
  if (it == reader_to_receiver_.end()) {
    return false;
  }
  delete it->second;
  reader_to_receiver_.erase(it);
  return true;
}

blink::WebMediaStreamTrack VideoSourceHandler::GetFirstVideoTrack(
    const std::string& url) {
  blink::WebMediaStream stream;
  if (registry_) {
    stream = registry_->GetMediaStream(url);
  } else {
    stream =
        blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));
  }

  if (stream.isNull()) {
    LOG(ERROR) << "GetFirstVideoSource - invalid url: " << url;
    return blink::WebMediaStreamTrack();
  }

  // Get the first video track from the stream.
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty()) {
    LOG(ERROR) << "GetFirstVideoSource - non video tracks available."
               << " url: " << url;
    return blink::WebMediaStreamTrack();
  }

  return video_tracks[0];
}

MediaStreamVideoSink* VideoSourceHandler::GetReceiver(
    FrameReaderInterface* reader) {
  SourceInfoMap::iterator it = reader_to_receiver_.find(reader);
  if (it == reader_to_receiver_.end()) {
    return NULL;
  }
  return it->second->receiver_.get();
}

VideoSourceHandler::SourceInfo::SourceInfo(
    const blink::WebMediaStreamTrack& blink_track,
    FrameReaderInterface* reader)
    : receiver_(new PpFrameReceiver()),
      track_(blink_track) {
  MediaStreamVideoSink::AddToVideoTrack(receiver_.get(), track_);
  receiver_->SetReader(reader);
}

VideoSourceHandler::SourceInfo::~SourceInfo() {
  MediaStreamVideoSink::RemoveFromVideoTrack(receiver_.get(), track_);
  receiver_->SetReader(NULL);
}

}  // namespace content

