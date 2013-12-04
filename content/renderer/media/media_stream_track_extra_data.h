// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_EXTRA_DATA_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace webrtc {
class MediaStreamTrackInterface;
}  // namespace webrtc

namespace content {

class CONTENT_EXPORT MediaStreamTrackExtraData
    : NON_EXPORTED_BASE(public blink::WebMediaStreamTrack::ExtraData) {
 public:
  MediaStreamTrackExtraData(webrtc::MediaStreamTrackInterface* track,
                            bool is_local_track);
  virtual ~MediaStreamTrackExtraData();

  const scoped_refptr<webrtc::MediaStreamTrackInterface>& track() const {
    return track_;
  }
  bool is_local_track () const { return is_local_track_; }

 private:
  scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  const bool is_local_track_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamTrackExtraData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_EXTRA_DATA_H_
