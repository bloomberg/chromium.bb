// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_track_source.h"

#include "base/trace_event/trace_event.h"
#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace content {

WebRtcVideoTrackSource::WebRtcVideoTrackSource(
    bool is_screencast,
    absl::optional<bool> needs_denoising)
    : AdaptedVideoTrackSource(/*required_alignment=*/1),
      is_screencast_(is_screencast),
      needs_denoising_(needs_denoising) {
  DETACH_FROM_THREAD(thread_checker_);
}

WebRtcVideoTrackSource::~WebRtcVideoTrackSource() = default;

WebRtcVideoTrackSource::SourceState WebRtcVideoTrackSource::state() const {
  // TODO(nisse): What's supposed to change this state?
  return MediaSourceInterface::SourceState::kLive;
}

bool WebRtcVideoTrackSource::remote() const {
  return false;
}

bool WebRtcVideoTrackSource::is_screencast() const {
  return is_screencast_;
}

absl::optional<bool> WebRtcVideoTrackSource::needs_denoising() const {
  return needs_denoising_;
}

void WebRtcVideoTrackSource::OnFrameCaptured(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("media", "WebRtcVideoSource::OnFrameCaptured");
  if (!(frame->IsMappable() &&
        (frame->format() == media::PIXEL_FORMAT_I420 ||
         frame->format() == media::PIXEL_FORMAT_I420A)) &&
      !frame->HasTextures()) {
    // Since connecting sources and sinks do not check the format, we need to
    // just ignore formats that we can not handle.
    LOG(ERROR) << "We cannot send frame with storage type: "
               << frame->AsHumanReadableString();
    NOTREACHED();
    return;
  }

  const int orig_width = frame->natural_size().width();
  const int orig_height = frame->natural_size().height();
  const int64_t now_us = rtc::TimeMicros();
  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  if (!AdaptFrame(orig_width, orig_height, now_us, &adapted_width,
                  &adapted_height, &crop_width, &crop_height, &crop_x,
                  &crop_y)) {
    return;
  }

  const int64_t translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(frame->timestamp().InMicroseconds(),
                                            now_us);

  // Return |frame| directly if it is texture backed, because there is no
  // cropping support for texture yet. See http://crbug/503653.
  if (frame->HasTextures()) {
    OnFrame(webrtc::VideoFrame(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(frame),
        webrtc::kVideoRotation_0, translated_camera_time_us));
    return;
  }

  // Translate crop rectangle from natural size to visible size.
  gfx::Rect cropped_visible_rect(
      frame->visible_rect().x() +
          crop_x * frame->visible_rect().width() / orig_width,
      frame->visible_rect().y() +
          crop_y * frame->visible_rect().height() / orig_height,
      crop_width * frame->visible_rect().width() / orig_width,
      crop_height * frame->visible_rect().height() / orig_height);

  const gfx::Size adapted_size(adapted_width, adapted_height);
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapVideoFrame(frame, frame->format(),
                                        cropped_visible_rect, adapted_size);
  if (!video_frame)
    return;

  video_frame->AddDestructionObserver(base::BindOnce(
      base::DoNothing::Once<scoped_refptr<media::VideoFrame>>(), frame));

  // If no scaling is needed, return a wrapped version of |frame| directly.
  if (video_frame->natural_size() == video_frame->visible_rect().size()) {
    OnFrame(webrtc::VideoFrame(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(video_frame),
        webrtc::kVideoRotation_0, translated_camera_time_us));
    return;
  }

  // We need to scale the frame before we hand it over to webrtc.
  const bool has_alpha = video_frame->format() == media::PIXEL_FORMAT_I420A;
  scoped_refptr<media::VideoFrame> scaled_frame =
      scaled_frame_pool_.CreateFrame(
          has_alpha ? media::PIXEL_FORMAT_I420A : media::PIXEL_FORMAT_I420,
          adapted_size, gfx::Rect(adapted_size), adapted_size,
          frame->timestamp());
  libyuv::I420Scale(video_frame->visible_data(media::VideoFrame::kYPlane),
                    video_frame->stride(media::VideoFrame::kYPlane),
                    video_frame->visible_data(media::VideoFrame::kUPlane),
                    video_frame->stride(media::VideoFrame::kUPlane),
                    video_frame->visible_data(media::VideoFrame::kVPlane),
                    video_frame->stride(media::VideoFrame::kVPlane),
                    video_frame->visible_rect().width(),
                    video_frame->visible_rect().height(),
                    scaled_frame->data(media::VideoFrame::kYPlane),
                    scaled_frame->stride(media::VideoFrame::kYPlane),
                    scaled_frame->data(media::VideoFrame::kUPlane),
                    scaled_frame->stride(media::VideoFrame::kUPlane),
                    scaled_frame->data(media::VideoFrame::kVPlane),
                    scaled_frame->stride(media::VideoFrame::kVPlane),
                    adapted_width, adapted_height, libyuv::kFilterBilinear);
  if (has_alpha) {
    libyuv::ScalePlane(video_frame->visible_data(media::VideoFrame::kAPlane),
                       video_frame->stride(media::VideoFrame::kAPlane),
                       video_frame->visible_rect().width(),
                       video_frame->visible_rect().height(),
                       scaled_frame->data(media::VideoFrame::kAPlane),
                       scaled_frame->stride(media::VideoFrame::kAPlane),
                       adapted_width, adapted_height, libyuv::kFilterBilinear);
  }

  OnFrame(webrtc::VideoFrame(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(scaled_frame),
      webrtc::kVideoRotation_0, translated_camera_time_us));
}

}  // namespace content
