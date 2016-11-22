// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/test_media_stream_renderer_factory.h"

#include "content/shell/renderer/layout_test/test_media_stream_video_renderer.h"
#include "media/media_features.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

using namespace blink;

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFrameDurationMs = 33;

bool IsMockMediaStreamWithVideo(const WebMediaStream& web_stream) {
#if BUILDFLAG(ENABLE_WEBRTC)
  if (web_stream.isNull())
    return false;
  WebVector<WebMediaStreamTrack> video_tracks;
  web_stream.videoTracks(video_tracks);
  return video_tracks.size() > 0;
#else
  return false;
#endif
}

}  // namespace

namespace content {

TestMediaStreamRendererFactory::TestMediaStreamRendererFactory() {}

TestMediaStreamRendererFactory::~TestMediaStreamRendererFactory() {}

scoped_refptr<MediaStreamVideoRenderer>
TestMediaStreamRendererFactory::GetVideoRenderer(
    const blink::WebMediaStream& web_stream,
    const base::Closure& error_cb,
    const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  if (!IsMockMediaStreamWithVideo(web_stream))
    return NULL;

  return new TestMediaStreamVideoRenderer(
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs),
      error_cb,
      repaint_cb);
}

scoped_refptr<MediaStreamAudioRenderer>
TestMediaStreamRendererFactory::GetAudioRenderer(
    const blink::WebMediaStream& web_stream,
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  return NULL;
}

}  // namespace content
