// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_FFMPEG_H_
#define MEDIA_BASE_MOCK_FFMPEG_H_

// TODO(scherkus): See if we can remove ffmpeg_common from this file.
#include "media/ffmpeg/ffmpeg_common.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockFFmpeg {
 public:
  MockFFmpeg();
  virtual ~MockFFmpeg();

  // TODO(ajwong): Organize this class, and make sure that all mock entrypoints
  // are still used.
  MOCK_METHOD0(AVCodecInit, void());
  MOCK_METHOD2(AVRegisterProtocol2, int(URLProtocol* protocol, int size));
  MOCK_METHOD0(AVRegisterAll, void());
  MOCK_METHOD1(AVRegisterLockManager, int(int (*cb)(void**, enum AVLockOp)));

  MOCK_METHOD1(AVCodecFindDecoder, AVCodec*(enum CodecID id));
  MOCK_METHOD2(AVCodecOpen, int(AVCodecContext* avctx, AVCodec* codec));
  MOCK_METHOD1(AVCodecClose, int(AVCodecContext* avctx));
  MOCK_METHOD2(AVCodecThreadInit, int(AVCodecContext* avctx, int threads));
  MOCK_METHOD1(AVCodecFlushBuffers, void(AVCodecContext* avctx));
  MOCK_METHOD0(AVCodecAllocContext, AVCodecContext*());
  MOCK_METHOD0(AVCodecAllocFrame, AVFrame*());
  MOCK_METHOD4(AVCodecDecodeVideo2,
               int(AVCodecContext* avctx, AVFrame* picture,
                   int* got_picture_ptr, AVPacket* avpkt));
  MOCK_METHOD1(AVBitstreamFilterInit,
               AVBitStreamFilterContext*(const char *name));
  MOCK_METHOD8(AVBitstreamFilterFilter,
               int(AVBitStreamFilterContext* bsfc, AVCodecContext* avctx,
                   const char* args, uint8_t** poutbuf, int* poutbuf_size,
                   const uint8_t* buf, int buf_size, int keyframe));
  MOCK_METHOD1(AVBitstreamFilterClose, void(AVBitStreamFilterContext* bsf));
  MOCK_METHOD1(AVDestructPacket, void(AVPacket* packet));

  MOCK_METHOD5(AVOpenInputFile, int(AVFormatContext** format,
                                    const char* filename,
                                    AVInputFormat* input_format,
                                    int buffer_size,
                                    AVFormatParameters* parameters));
  MOCK_METHOD1(AVCloseInputFile, void(AVFormatContext* format));
  MOCK_METHOD1(AVFindStreamInfo, int(AVFormatContext* format));
  MOCK_METHOD2(AVReadFrame, int(AVFormatContext* format, AVPacket* packet));
  MOCK_METHOD4(AVSeekFrame, int(AVFormatContext *format,
                                int stream_index,
                                int64_t timestamp,
                                int flags));

  MOCK_METHOD1(AVInitPacket, void(AVPacket* pkt));
  MOCK_METHOD2(AVNewPacket, int(AVPacket* packet, int size));
  MOCK_METHOD1(AVFreePacket, void(AVPacket* packet));
  MOCK_METHOD1(AVFree, void(void* ptr));
  MOCK_METHOD1(AVDupPacket, int(AVPacket* packet));

  MOCK_METHOD1(AVLogSetLevel, void(int level));

  // Used for verifying check points during tests.
  MOCK_METHOD1(CheckPoint, void(int id));

  // Returns the current MockFFmpeg instance.
  static MockFFmpeg* get();

  // Returns the URLProtocol registered by the FFmpegGlue singleton.
  static URLProtocol* protocol();

  // AVPacket destructor for packets allocated by av_new_packet().
  static void DestructPacket(AVPacket* packet);

  // Modifies the number of outstanding packets.
  void inc_outstanding_packets();
  void dec_outstanding_packets();

 private:
  static MockFFmpeg* instance_;
  static URLProtocol* protocol_;

  // Tracks the number of packets allocated by calls to av_read_frame() and
  // av_free_packet().  We crash the unit test if this is not zero at time of
  // destruction.
  int outstanding_packets_;
};

// Used for simulating av_read_frame().
ACTION_P3(CreatePacket, stream_index, data, size) {
  // Confirm we're dealing with AVPacket so we can safely const_cast<>.
  ::testing::StaticAssertTypeEq<AVPacket*, arg1_type>();
  memset(arg1, 0, sizeof(*arg1));
  arg1->stream_index = stream_index;
  arg1->data = const_cast<uint8*>(data);
  arg1->size = size;

  // Increment number of packets allocated.
  MockFFmpeg::get()->inc_outstanding_packets();

  return 0;
}

// Used for simulating av_read_frame().
ACTION_P3(CreatePacketNoCount, stream_index, data, size) {
  // Confirm we're dealing with AVPacket so we can safely const_cast<>.
  ::testing::StaticAssertTypeEq<AVPacket*, arg1_type>();
  memset(arg1, 0, sizeof(*arg1));
  arg1->stream_index = stream_index;
  arg1->data = const_cast<uint8*>(data);
  arg1->size = size;

  return 0;
}

// Used for simulating av_new_packet().
ACTION(NewPacket) {
  ::testing::StaticAssertTypeEq<AVPacket*, arg0_type>();
  int size = arg1;
  memset(arg0, 0, sizeof(*arg0));
  arg0->data = new uint8[size];
  arg0->size = size;
  arg0->destruct = &MockFFmpeg::DestructPacket;

  // Increment number of packets allocated.
  MockFFmpeg::get()->inc_outstanding_packets();

  return 0;
}

// Used for simulating av_free_packet().
ACTION(FreePacket) {
  ::testing::StaticAssertTypeEq<AVPacket*, arg0_type>();

  // Call the destructor if present, such as the one assigned in NewPacket().
  if (arg0->destruct) {
    arg0->destruct(arg0);
  }

  // Decrement number of packets allocated.
  MockFFmpeg::get()->dec_outstanding_packets();
}

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FFMPEG_H_
