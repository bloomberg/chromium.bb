// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "media/base/video_decoder.h"
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

  // media::VideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<media::DemuxerStream>& stream,
                          const media::PipelineStatusCB& status_cb,
                          const media::StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& clusure) OVERRIDE;
  virtual void Stop(const base::Closure& clusure) OVERRIDE;
  virtual const gfx::Size& natural_size() OVERRIDE;
  virtual void PrepareForShutdownHack() OVERRIDE;

  // cricket::VideoRenderer implementation
  virtual bool SetSize(int width, int height, int reserved) OVERRIDE;
  virtual bool RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

 protected:
  virtual ~RTCVideoDecoder();

 private:
  friend class RTCVideoDecoderTest;
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, Initialize_Successful);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoReset);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoSetSize);

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kStopped
  };

  void CancelPendingRead();

  MessageLoop* message_loop_;
  gfx::Size visible_size_;
  std::string url_;
  DecoderState state_;
  ReadCB read_cb_;
  bool got_first_frame_;
  bool shutting_down_;
  base::TimeDelta last_frame_timestamp_;
  base::TimeDelta start_time_;

  // Used for accessing |read_cb_| from another thread.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoder);
};

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
