// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include "base/string_util.h"
#include "content/renderer/media/capture_video_decoder.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "googleurl/src/gurl.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFramePerSecond = 30;

static const int kStartOpenSessionId = 1;

// TODO(wjia): remove this string when full media stream code is checked in.
static const char kRawMediaScheme[] = "mediastream";

}  // namespace

MediaStreamImpl::MediaStreamImpl(VideoCaptureImplManager* vc_manager)
    : vc_manager_(vc_manager) {
}

MediaStreamImpl::~MediaStreamImpl() {}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::GetVideoDecoder(
    const GURL& url, media::MessageLoopFactory* message_loop_factory) {
  bool raw_media = (url.spec().find(kRawMediaScheme) == 0);
  media::VideoDecoder* decoder = NULL;
  if (raw_media) {
    media::VideoCapture::VideoCaptureCapability capability;
    capability.width = kVideoCaptureWidth;
    capability.height = kVideoCaptureHeight;
    capability.max_fps = kVideoCaptureFramePerSecond;
    capability.expected_capture_delay = 0;
    capability.raw_type = media::VideoFrame::I420;
    capability.interlaced = false;

    decoder = new CaptureVideoDecoder(
        message_loop_factory->GetMessageLoopProxy("CaptureVideoDecoder").get(),
        kStartOpenSessionId, vc_manager_.get(), capability);
  }
  return decoder;
}
