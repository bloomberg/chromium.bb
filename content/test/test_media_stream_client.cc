// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_media_stream_client.h"

#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/test/test_video_frame_provider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

using namespace WebKit;

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

TestMediaStreamClient::TestMediaStreamClient(RenderView* render_view)
    : RenderViewObserver(render_view) {}

TestMediaStreamClient::~TestMediaStreamClient() {}

bool TestMediaStreamClient::IsMediaStream(const GURL& url) {
  return IsMockMediaStreamWithVideo(url);
}

scoped_refptr<VideoFrameProvider> TestMediaStreamClient::GetVideoFrameProvider(
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

scoped_refptr<MediaStreamAudioRenderer> TestMediaStreamClient::GetAudioRenderer(
    const GURL& url) {
  return NULL;
}

}  // namespace content
