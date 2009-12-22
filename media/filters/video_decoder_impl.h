// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_DECODER_IMPL_H_
#define MEDIA_FILTERS_VIDEO_DECODER_IMPL_H_

#include "base/time.h"
#include "media/base/pts_heap.h"
#include "media/filters/decoder_base.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// FFmpeg types.
struct AVFrame;
struct AVRational;

namespace media {

class VideoDecodeEngine;

class VideoDecoderImpl : public DecoderBase<VideoDecoder, VideoFrame> {
 protected:
  VideoDecoderImpl(VideoDecodeEngine* engine);

  // Make this class abstract to document that this class cannot be used
  // directly as a filter type because it does not implement the static methods
  // CreateFactory() and IsMediaFormatSupported().
  //
  // TODO(ajwong): When we clean up the filters to not required a static
  // implementation of CreateFactory() and IsMediaFormatSupported(), this
  // class doesn't probably need to be abstract.
  virtual ~VideoDecoderImpl() = 0;

 private:
  friend class FilterFactoryImpl1<VideoDecoderImpl, VideoDecodeEngine*>;
  friend class DecoderPrivateMock;
  friend class VideoDecoderImplTest;
  FRIEND_TEST(VideoDecoderImplTest, FindPtsAndDuration);
  FRIEND_TEST(VideoDecoderImplTest, DoDecode_EnqueueVideoFrameError);
  FRIEND_TEST(VideoDecoderImplTest, DoDecode_FinishEnqueuesEmptyFrames);
  FRIEND_TEST(VideoDecoderImplTest, DoDecode_TestStateTransition);
  FRIEND_TEST(VideoDecoderImplTest, DoSeek);

  // The TimeTuple struct is used to hold the needed timestamp data needed for
  // enqueuing a video frame.
  struct TimeTuple {
    base::TimeDelta timestamp;
    base::TimeDelta duration;
  };

  enum DecoderState {
    kNormal,
    kFlushCodec,
    kDecodeFinished,
  };

  // Implement DecoderBase template methods.
  virtual void DoInitialize(DemuxerStream* demuxer_stream, bool* success,
                            Task* done_cb);
  virtual void DoSeek(base::TimeDelta time, Task* done_cb);
  virtual void DoDecode(Buffer* buffer, Task* done_cb);

  virtual bool EnqueueVideoFrame(VideoSurface::Format surface_format,
                                 const TimeTuple& time,
                                 const AVFrame* frame);

  // Create an empty video frame and queue it.
  virtual void EnqueueEmptyFrame();

  virtual void CopyPlane(size_t plane, const VideoSurface& surface,
                         const AVFrame* frame);

  // Methods that pickup after the decode engine has finished its action.
  virtual void OnInitializeComplete(bool* success /* Not owned */,
                                    Task* done_cb);
  virtual void OnDecodeComplete(AVFrame* yuv_frame, bool* got_frame,
                                Task* done_cb);

  // Attempt to get the PTS and Duration for this frame by examining the time
  // info provided via packet stream (stored in |pts_heap|), or the info
  // writen into the AVFrame itself.  If no data is available in either, then
  // attempt to generate a best guess of the pts based on the last known pts.
  //
  // Data inside the AVFrame (if provided) is trusted the most, followed
  // by data from the packet stream.  Estimation based on the |last_pts| is
  // reserved as a last-ditch effort.
  virtual TimeTuple FindPtsAndDuration(const AVRational& time_base,
                                       const PtsHeap& pts_heap,
                                       const TimeTuple& last_pts,
                                       const AVFrame* frame);

  // Signals the pipeline that a decode error occurs, and moves the decoder
  // into the kDecodeFinished state.
  virtual void SignalPipelineError();

  // Injection point for unittest to provide a mock engine.  Takes ownership of
  // the provided engine.
  virtual void SetVideoDecodeEngineForTest(VideoDecodeEngine* engine);

  size_t width_;
  size_t height_;

  PtsHeap pts_heap_;  // Heap of presentation timestamps.
  TimeTuple last_pts_;
  scoped_ptr<AVRational> time_base_;  // Pointer to avoid needing full type.
  DecoderState state_;
  scoped_ptr<VideoDecodeEngine> decode_engine_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_DECODER_IMPL_H_
