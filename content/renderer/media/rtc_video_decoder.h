// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"
#include "third_party/libjingle/source/talk/session/phone/mediachannel.h"
#include "third_party/libjingle/source/talk/session/phone/videorenderer.h"

class MessageLoop;

namespace cricket {
class VideoFrame;
}  // namespace cricket

class CONTENT_EXPORT RTCVideoDecoder
    : public media::VideoDecoder,
      NON_EXPORTED_BASE(public cricket::VideoRenderer) {
 public:
  RTCVideoDecoder(MessageLoop* message_loop, const std::string& url);
  virtual ~RTCVideoDecoder();

  // Filter implementation.
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time,
                    const media::FilterStatusCB& cb) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;

  // Decoder implementation.
  virtual void Initialize(
      media::DemuxerStream* demuxer_stream,
      const base::Closure& filter_callback,
      const media::StatisticsCallback& stat_callback) OVERRIDE;
  virtual void Read(const ReadCB& callback) OVERRIDE;
  virtual const gfx::Size& natural_size() OVERRIDE;

  // cricket::VideoRenderer implementation
  virtual bool SetSize(int width, int height, int reserved) OVERRIDE;
  virtual bool RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

 private:
  friend class RTCVideoDecoderTest;
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, Initialize_Successful);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoSeek);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoFlush);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoSetSize);

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kPaused,
    kStopped
  };

  MessageLoop* message_loop_;
  gfx::Size visible_size_;
  std::string url_;
  DecoderState state_;
  ReadCB read_cb_;

  // Used for accessing |read_cb_| from another thread.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoder);
};

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
