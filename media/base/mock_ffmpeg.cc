// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_ffmpeg.h"

#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

MockFFmpeg* MockFFmpeg::instance_ = NULL;
URLProtocol* MockFFmpeg::protocol_ = NULL;

MockFFmpeg::MockFFmpeg()
    : outstanding_packets_(0) {
  CHECK(instance_ == NULL) << "Only a single MockFFmpeg instance can exist";
  instance_ = this;

  // If we haven't assigned our static copy of URLProtocol, set up expectations
  // to catch the URLProtocol registered when the singleton instance of
  // FFmpegGlue is created.
  //
  // TODO(scherkus): this feels gross and I need to think of a way to better
  // inject/mock singletons.
  if (!protocol_) {
    EXPECT_CALL(*this, AVLogSetLevel(AV_LOG_QUIET))
        .Times(AtMost(1))
        .WillOnce(Return());
    EXPECT_CALL(*this, AVCodecInit())
        .Times(AtMost(1))
        .WillOnce(Return());
    EXPECT_CALL(*this, AVRegisterProtocol2(_,_))
        .Times(AtMost(1))
        .WillOnce(DoAll(SaveArg<0>(&protocol_), Return(0)));
    EXPECT_CALL(*this, AVRegisterAll())
        .Times(AtMost(1))
        .WillOnce(Return());
  }
}

MockFFmpeg::~MockFFmpeg() {
  CHECK(!outstanding_packets_)
      << "MockFFmpeg destroyed with outstanding packets";
  CHECK(instance_);
  instance_ = NULL;
}

void MockFFmpeg::inc_outstanding_packets() {
  ++outstanding_packets_;
}

void MockFFmpeg::dec_outstanding_packets() {
  CHECK(outstanding_packets_ > 0);
  --outstanding_packets_;
}

// static
MockFFmpeg* MockFFmpeg::get() {
  return instance_;
}

// static
URLProtocol* MockFFmpeg::protocol() {
  return protocol_;
}

// static
void MockFFmpeg::DestructPacket(AVPacket* packet) {
  delete [] packet->data;
  packet->data = NULL;
  packet->size = 0;
}

// FFmpeg stubs that delegate to the FFmpegMock instance.
extern "C" {
void avcodec_init() {
  MockFFmpeg::get()->AVCodecInit();
}

int av_register_protocol2(URLProtocol* protocol, int size) {
  return MockFFmpeg::get()->AVRegisterProtocol2(protocol, size);
}

void av_register_all() {
  MockFFmpeg::get()->AVRegisterAll();
}

int av_lockmgr_register(int (*cb)(void**, enum AVLockOp)) {
  // Here |mock| may be NULL when this function is called from ~FFmpegGlue().
  if (MockFFmpeg::get()) {
    return MockFFmpeg::get()->AVRegisterLockManager(cb);
  }
  return 0;
}

AVCodec* avcodec_find_decoder(enum CodecID id) {
  return MockFFmpeg::get()->AVCodecFindDecoder(id);
}

int avcodec_open(AVCodecContext* avctx, AVCodec* codec) {
  return MockFFmpeg::get()->AVCodecOpen(avctx, codec);
}

int avcodec_close(AVCodecContext* avctx) {
  return MockFFmpeg::get()->AVCodecClose(avctx);
}

int avcodec_thread_init(AVCodecContext* avctx, int threads) {
  return MockFFmpeg::get()->AVCodecThreadInit(avctx, threads);
}

void avcodec_flush_buffers(AVCodecContext* avctx) {
  return MockFFmpeg::get()->AVCodecFlushBuffers(avctx);
}

AVCodecContext* avcodec_alloc_context() {
  return MockFFmpeg::get()->AVCodecAllocContext();
}

AVFrame* avcodec_alloc_frame() {
  return MockFFmpeg::get()->AVCodecAllocFrame();
}

int avcodec_decode_video2(AVCodecContext* avctx, AVFrame* picture,
                          int* got_picture_ptr, AVPacket* avpkt) {
  return MockFFmpeg::get()->
      AVCodecDecodeVideo2(avctx, picture, got_picture_ptr, avpkt);
}

AVBitStreamFilterContext* av_bitstream_filter_init(const char* name) {
  return MockFFmpeg::get()->AVBitstreamFilterInit(name);
}

int av_bitstream_filter_filter(AVBitStreamFilterContext* bsfc,
                               AVCodecContext* avctx,
                               const char* args,
                               uint8_t** poutbuf,
                               int* poutbuf_size,
                               const uint8_t* buf,
                               int buf_size,
                               int keyframe) {
  return MockFFmpeg::get()->
      AVBitstreamFilterFilter(bsfc, avctx, args, poutbuf, poutbuf_size, buf,
                              buf_size, keyframe);
}

void av_bitstream_filter_close(AVBitStreamFilterContext* bsf) {
  return MockFFmpeg::get()->AVBitstreamFilterClose(bsf);
}

int av_open_input_file(AVFormatContext** format, const char* filename,
                       AVInputFormat* input_format, int buffer_size,
                       AVFormatParameters* parameters) {
  return MockFFmpeg::get()->AVOpenInputFile(format, filename,
                                            input_format, buffer_size,
                                            parameters);
}

void av_close_input_file(AVFormatContext* format) {
  MockFFmpeg::get()->AVCloseInputFile(format);
}

int av_find_stream_info(AVFormatContext* format) {
  return MockFFmpeg::get()->AVFindStreamInfo(format);
}

int64 av_rescale_q(int64 a, AVRational bq, AVRational cq) {
  // Because this is a math function there's little point in mocking it, so we
  // implement a cheap version that's capable of overflowing.
  int64 num = bq.num * cq.den;
  int64 den = cq.num * bq.den;
  return a * num / den;
}

int av_read_frame(AVFormatContext* format, AVPacket* packet) {
  return MockFFmpeg::get()->AVReadFrame(format, packet);
}

int av_seek_frame(AVFormatContext *format, int stream_index, int64_t timestamp,
                  int flags) {
  return MockFFmpeg::get()->AVSeekFrame(format, stream_index, timestamp,
                                        flags);
}

void av_init_packet(AVPacket* pkt) {
  return MockFFmpeg::get()->AVInitPacket(pkt);
}

int av_new_packet(AVPacket* packet, int size) {
  return MockFFmpeg::get()->AVNewPacket(packet, size);
}

void av_free_packet(AVPacket* packet) {
  MockFFmpeg::get()->AVFreePacket(packet);
}

void av_free(void* ptr) {
  // Freeing NULL pointers are valid, but they aren't interesting from a mock
  // perspective.
  if (ptr) {
    MockFFmpeg::get()->AVFree(ptr);
  }
}

int av_dup_packet(AVPacket* packet) {
  return MockFFmpeg::get()->AVDupPacket(packet);
}

void av_log_set_level(int level) {
  MockFFmpeg::get()->AVLogSetLevel(level);
}

void av_destruct_packet(AVPacket *pkt) {
  MockFFmpeg::get()->AVDestructPacket(pkt);
}

}  // extern "C"

}  // namespace media
