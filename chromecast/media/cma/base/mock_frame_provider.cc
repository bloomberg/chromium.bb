// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/mock_frame_provider.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/frame_generator_for_test.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace chromecast {
namespace media {

MockFrameProvider::MockFrameProvider() {
}

MockFrameProvider::~MockFrameProvider() {
}

void MockFrameProvider::Configure(
    const std::vector<bool>& delayed_task_pattern,
    scoped_ptr<FrameGeneratorForTest> frame_generator) {
  delayed_task_pattern_ = delayed_task_pattern;
  pattern_idx_ = 0;

  frame_generator_.reset(frame_generator.release());
}

void MockFrameProvider::Read(const ReadCB& read_cb) {
  bool delayed = delayed_task_pattern_[pattern_idx_];
  pattern_idx_ = (pattern_idx_ + 1) % delayed_task_pattern_.size();

  if (delayed) {
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MockFrameProvider::DoRead,
                   base::Unretained(this), read_cb),
        base::TimeDelta::FromMilliseconds(1));
  } else {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockFrameProvider::DoRead,
                   base::Unretained(this), read_cb));
  }
}

void MockFrameProvider::Flush(const base::Closure& flush_cb) {
  flush_cb.Run();
}

void MockFrameProvider::DoRead(const ReadCB& read_cb) {
  bool has_config = frame_generator_->HasDecoderConfig();

  scoped_refptr<DecoderBufferBase> buffer(frame_generator_->Generate());
  ASSERT_TRUE(buffer.get());

  ::media::AudioDecoderConfig audio_config;
  ::media::VideoDecoderConfig video_config;
  if (has_config) {
    gfx::Size coded_size(640, 480);
    gfx::Rect visible_rect(640, 480);
    gfx::Size natural_size(640, 480);
    video_config = ::media::VideoDecoderConfig(
        ::media::kCodecH264,
        ::media::VIDEO_CODEC_PROFILE_UNKNOWN,
        ::media::VideoFrame::YV12,
        coded_size,
        visible_rect,
        natural_size,
        NULL, 0,
        false);

    audio_config = ::media::AudioDecoderConfig(
      ::media::kCodecAAC,
      ::media::kSampleFormatS16,
      ::media::CHANNEL_LAYOUT_STEREO,
      44100,
      NULL, 0,
      false);
  }

  read_cb.Run(buffer, audio_config, video_config);
}

}  // namespace media
}  // namespace chromecast
