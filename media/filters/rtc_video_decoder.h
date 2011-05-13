// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_RTC_VIDEO_DECODER_H_
#define MEDIA_FILTERS_RTC_VIDEO_DECODER_H_

#include <deque>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"
#include "media/filters/decoder_base.h"

// TODO(ronghuawu) ExternalRenderer should be defined in WebRtc
class ExternalRenderer {
 public:
  virtual int FrameSizeChange(unsigned int width,
                              unsigned int height,
                              unsigned int number_of_streams) = 0;
  virtual int DeliverFrame(unsigned char* buffer, int buffer_size) = 0;

 protected:
  virtual ~ExternalRenderer() {}
};

namespace media {

class RTCVideoDecoder : public VideoDecoder,
                        public ExternalRenderer {
 public:
  RTCVideoDecoder(MessageLoop* message_loop, const std::string& url);
  virtual ~RTCVideoDecoder();

  // Filter implementation.
  virtual void Play(FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, const FilterStatusCB& cb);
  virtual void Pause(FilterCallback* callback);
  virtual void Stop(FilterCallback* callback);

  // Decoder implementation.
  virtual void Initialize(DemuxerStream* demuxer_stream,
                          FilterCallback* filter_callback,
                          StatisticsCallback* stat_callback);
  virtual const MediaFormat& media_format();
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> video_frame);
  virtual bool ProvidesBuffer();

  // ExternalRenderer implementation
  virtual int FrameSizeChange(unsigned int width,
                              unsigned int height,
                              unsigned int number_of_streams);

  virtual int DeliverFrame(unsigned char* buffer,
                           int buffer_size);

  // TODO(ronghuawu): maybe move this function to a
  // base class (RawVideoDecoder) so that the camera preview may share this.
  static bool IsUrlSupported(const std::string& url);

 private:
  friend class RTCVideoDecoderTest;
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, Initialize_Successful);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoSeek);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoDeliverFrame);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoFrameSizeChange);

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kSeeking,
    kPaused,
    kStopped
  };

  MessageLoop* message_loop_;
  size_t width_;
  size_t height_;
  std::string url_;
  DecoderState state_;
  MediaFormat media_format_;
  std::deque<scoped_refptr<VideoFrame> > frame_queue_available_;
  // Used for accessing frame queue from another thread.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_RTC_VIDEO_DECODER_H_
