// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_utils.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/guid.h"
#include "media/capture/video_capturer_source.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"

namespace content {

bool AddVideoTrackToMediaStream(
    std::unique_ptr<media::VideoCapturerSource> video_source,
    bool is_remote,
    blink::WebMediaStream* web_media_stream) {
  DCHECK(video_source.get());
  if (!web_media_stream || web_media_stream->IsNull()) {
    DLOG(ERROR) << "WebMediaStream is null";
    return false;
  }

  media::VideoCaptureFormats preferred_formats =
      video_source->GetPreferredFormats();
  blink::MediaStreamVideoSource* const media_stream_source =
      new blink::MediaStreamVideoCapturerSource(
          blink::WebPlatformMediaStreamSource::SourceStoppedCallback(),
          std::move(video_source));
  const blink::WebString track_id =
      blink::WebString::FromUTF8(base::GenerateGUID());
  blink::WebMediaStreamSource web_media_stream_source;
  web_media_stream_source.Initialize(
      track_id, blink::WebMediaStreamSource::kTypeVideo, track_id, is_remote);
  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetPlatformSource(
      base::WrapUnique(media_stream_source));
  web_media_stream_source.SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats,
      media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
      false /* is_device_capture */));
  web_media_stream->AddTrack(blink::MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_source, blink::MediaStreamVideoSource::ConstraintsCallback(),
      true));
  return true;
}

}  // namespace content
