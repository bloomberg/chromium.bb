// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  // av_lockmgr_register() is also called from ~FFmpegLock(), so we expect
  // it to be called at the end.
  EXPECT_CALL(*this, AVRegisterLockManager(_))
    .Times(AtMost(2))
    .WillRepeatedly(Return(0));
}

MockFFmpeg::~MockFFmpeg() {
  CHECK(!outstanding_packets_)
      << "MockFFmpeg destroyed with outstanding packets";
}

void MockFFmpeg::inc_outstanding_packets() {
  ++outstanding_packets_;
}

void MockFFmpeg::dec_outstanding_packets() {
  CHECK(outstanding_packets_ > 0);
  --outstanding_packets_;
}

// static
void MockFFmpeg::set(MockFFmpeg* instance) {
  instance_ = instance;
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
  media::MockFFmpeg::get()->AVCodecInit();
}

int av_register_protocol2(URLProtocol* protocol, int size) {
  return media::MockFFmpeg::get()->AVRegisterProtocol2(protocol, size);
}

void av_register_all() {
  media::MockFFmpeg::get()->AVRegisterAll();
}

int av_lockmgr_register(int (*cb)(void**, enum AVLockOp)) {
  media::MockFFmpeg* mock = media::MockFFmpeg::get();
  // Here |mock| may be NULL when this function is called from ~FFmpegGlue().
  if (mock != NULL) {
    return mock->AVRegisterLockManager(cb);
  } else {
    return 0;
  }
}

AVCodec* avcodec_find_decoder(enum CodecID id) {
  return media::MockFFmpeg::get()->AVCodecFindDecoder(id);
}

int avcodec_open(AVCodecContext* avctx, AVCodec* codec) {
  return media::MockFFmpeg::get()->AVCodecOpen(avctx, codec);
}

int avcodec_close(AVCodecContext* avctx) {
  return media::MockFFmpeg::get()->AVCodecClose(avctx);
}

int avcodec_thread_init(AVCodecContext* avctx, int threads) {
  return media::MockFFmpeg::get()->AVCodecThreadInit(avctx, threads);
}

void avcodec_flush_buffers(AVCodecContext* avctx) {
  return media::MockFFmpeg::get()->AVCodecFlushBuffers(avctx);
}

AVFrame* avcodec_alloc_frame() {
  return media::MockFFmpeg::get()->AVCodecAllocFrame();
}

int avcodec_decode_video2(AVCodecContext* avctx, AVFrame* picture,
                          int* got_picture_ptr, AVPacket* avpkt) {
  return media::MockFFmpeg::get()->
      AVCodecDecodeVideo2(avctx, picture, got_picture_ptr, avpkt);
}

AVBitStreamFilterContext* av_bitstream_filter_init(const char* name) {
  return media::MockFFmpeg::get()->AVBitstreamFilterInit(name);
}

int av_bitstream_filter_filter(AVBitStreamFilterContext* bsfc,
                               AVCodecContext* avctx,
                               const char* args,
                               uint8_t** poutbuf,
                               int* poutbuf_size,
                               const uint8_t* buf,
                               int buf_size,
                               int keyframe) {
  return media::MockFFmpeg::get()->
      AVBitstreamFilterFilter(bsfc, avctx, args, poutbuf, poutbuf_size, buf,
                              buf_size, keyframe);
}

void av_bitstream_filter_close(AVBitStreamFilterContext* bsf) {
  return media::MockFFmpeg::get()->AVBitstreamFilterClose(bsf);
}

int av_open_input_file(AVFormatContext** format, const char* filename,
                       AVInputFormat* input_format, int buffer_size,
                       AVFormatParameters* parameters) {
  return media::MockFFmpeg::get()->AVOpenInputFile(format, filename,
                                                   input_format, buffer_size,
                                                   parameters);
}

void av_close_input_file(AVFormatContext* format) {
  media::MockFFmpeg::get()->AVCloseInputFile(format);
}

int av_find_stream_info(AVFormatContext* format) {
  return media::MockFFmpeg::get()->AVFindStreamInfo(format);
}

int64 av_rescale_q(int64 a, AVRational bq, AVRational cq) {
  // Because this is a math function there's little point in mocking it, so we
  // implement a cheap version that's capable of overflowing.
  int64 num = bq.num * cq.den;
  int64 den = cq.num * bq.den;
  return a * num / den;
}

int av_read_frame(AVFormatContext* format, AVPacket* packet) {
  return media::MockFFmpeg::get()->AVReadFrame(format, packet);
}

int av_seek_frame(AVFormatContext *format, int stream_index, int64_t timestamp,
                  int flags) {
  return media::MockFFmpeg::get()->AVSeekFrame(format, stream_index, timestamp,
                                               flags);
}

void av_init_packet(AVPacket* pkt) {
  return media::MockFFmpeg::get()->AVInitPacket(pkt);
}

int av_new_packet(AVPacket* packet, int size) {
  return media::MockFFmpeg::get()->AVNewPacket(packet, size);
}

void av_free_packet(AVPacket* packet) {
  media::MockFFmpeg::get()->AVFreePacket(packet);
}

void av_free(void* ptr) {
  // Freeing NULL pointers are valid, but they aren't interesting from a mock
  // perspective.
  if (ptr) {
    media::MockFFmpeg::get()->AVFree(ptr);
  }
}

int av_dup_packet(AVPacket* packet) {
  return media::MockFFmpeg::get()->AVDupPacket(packet);
}

void av_log_set_level(int level) {
  media::MockFFmpeg::get()->AVLogSetLevel(level);
}

void av_destruct_packet(AVPacket *pkt) {
  media::MockFFmpeg::get()->AVDestructPacket(pkt);
}

}  // extern "C"

}  // namespace media
