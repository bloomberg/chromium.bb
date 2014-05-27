// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_media_stream_renderer_factory.h"

#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/test/test_video_frame_provider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

using namespace blink;

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFrameDurationMs = 33;

bool IsMockMediaStreamWithVideo(const WebURL& url) {
#if ENABLE_WEBRTC
  WebMediaStream descriptor(
      WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));
  if (descriptor.isNull())
    return false;
  WebVector<WebMediaStreamTrack> video_tracks;
  descriptor.videoTracks(video_tracks);
  return video_tracks.size() > 0;
#else
  return false;
#endif
}

}  // namespace

namespace content {

TestMediaStreamRendererFactory::TestMediaStreamRendererFactory() {}

TestMediaStreamRendererFactory::~TestMediaStreamRendererFactory() {}

scoped_refptr<VideoFrameProvider>
TestMediaStreamRendererFactory::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb) {
  if (!IsMockMediaStreamWithVideo(url))
    return NULL;

  return new TestVideoFrameProvider(
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs),
      error_cb,
      repaint_cb);
}

scoped_refptr<MediaStreamAudioRenderer>
TestMediaStreamRendererFactory::GetAudioRenderer(
    const GURL& url, int render_view_id, int render_frame_id) {
  return NULL;
}

}  // namespace content
