// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_media_stream_client.h"

#include "googleurl/src/gurl.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "webkit/renderer/media/media_stream_audio_renderer.h"
#include "webkit/renderer/media/simple_video_frame_provider.h"

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
  WebVector<WebMediaStreamTrack> videoSources;
  descriptor.videoSources(videoSources);
  return videoSources.size() > 0;
#else
  return false;
#endif
}

}  // namespace

namespace content {

ShellMediaStreamClient::ShellMediaStreamClient() {}

ShellMediaStreamClient::~ShellMediaStreamClient() {}

bool ShellMediaStreamClient::IsMediaStream(const GURL& url) {
  return IsMockMediaStreamWithVideo(url);
}

scoped_refptr<webkit_media::VideoFrameProvider>
ShellMediaStreamClient::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) {
  if (!IsMockMediaStreamWithVideo(url))
    return NULL;

  return new webkit_media::SimpleVideoFrameProvider(
      gfx::Size(kVideoCaptureWidth, kVideoCaptureHeight),
      base::TimeDelta::FromMilliseconds(kVideoCaptureFrameDurationMs),
      error_cb,
      repaint_cb);
}

scoped_refptr<webkit_media::MediaStreamAudioRenderer>
ShellMediaStreamClient::GetAudioRenderer(const GURL& url) {
  return NULL;
}

}  // namespace content
