// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"

namespace webrtc {
class MediaStreamInterface;
}  // namespace webrtc

namespace content {

class MediaStreamDependencyFactory;

// MediaStream is the Chrome representation of blink::WebMediaStream.
// It is owned by blink::WebMediaStream as blink::WebMediaStream::ExtraData.
class CONTENT_EXPORT MediaStream
    : NON_EXPORTED_BASE(public blink::WebMediaStream::ExtraData) {
 public:
  // Constructor for local MediaStreams.
  MediaStream(MediaStreamDependencyFactory* factory,
              const blink::WebMediaStream& stream);
  // Constructor for remote MediaStreams.
  explicit MediaStream(webrtc::MediaStreamInterface* stream);

  virtual ~MediaStream();

  // Returns an instance of MediaStream. This method will never return NULL.
  static MediaStream* GetMediaStream(
      const blink::WebMediaStream& stream);

  // Returns a libjingle representation of a MediaStream. If a representation
  // does not exist- the libjingle stream is created. This method will never
  // return NULL.
  static webrtc::MediaStreamInterface* GetAdapter(
      const blink::WebMediaStream& stream);

  // TODO(xians): Remove |is_local| once AudioTracks can be rendered the same
  // way regardless if they are local or remote.
  bool is_local() const { return is_local_; }

  // Called by MediaStreamCenter when a track has been added to a stream stream.
  // If a libjingle representation of |stream| exist, the track is added to
  // the libjingle MediaStream.
  bool AddTrack(const blink::WebMediaStream& stream,
                const blink::WebMediaStreamTrack& track);

  // Called by MediaStreamCenter when a track has been removed from |stream|
  // If a libjingle representation or |stream| exist, the track is removed
  // from the libjingle MediaStream.
  bool RemoveTrack(const blink::WebMediaStream& stream,
                   const blink::WebMediaStreamTrack& track);

 protected:
  virtual webrtc::MediaStreamInterface* GetWebRtcAdapter(
      const blink::WebMediaStream& stream);

 private:
  scoped_refptr<webrtc::MediaStreamInterface> stream_adapter_;
  const bool is_local_;
  const std::string label_;

  // Weak ref to a MediaStreamDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  MediaStreamDependencyFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStream);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_
