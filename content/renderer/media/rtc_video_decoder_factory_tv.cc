// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder_factory_tv.h"

#include "base/callback_helpers.h"
#include "content/renderer/media/rtc_video_decoder_bridge_tv.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "third_party/libjingle/source/talk/base/ratetracker.h"

using media::DemuxerStream;

namespace content {

// RTCDemuxerStream ------------------------------------------------------------

class RTCDemuxerStream : public DemuxerStream {
 public:
  explicit RTCDemuxerStream(const gfx::Size& size);
  virtual ~RTCDemuxerStream();

  // DemuxerStream implementation.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual media::AudioDecoderConfig audio_decoder_config() OVERRIDE;
  virtual media::VideoDecoderConfig video_decoder_config() OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;

  void QueueBuffer(scoped_refptr<media::DecoderBuffer> buffer,
                   const gfx::Size& new_size);
  void Destroy();

 private:
  struct BufferEntry {
    BufferEntry(const scoped_refptr<media::DecoderBuffer>& decoder_buffer_param,
                const gfx::Size& new_size_param)
        : decoder_buffer(decoder_buffer_param),
          new_size(new_size_param) {}

    scoped_refptr<media::DecoderBuffer> decoder_buffer;
    // When |!new_size.isEmpty()|, it means that config change with new size
    // |new_size| happened.
    gfx::Size new_size;
  };

  void RunReadCallback_Locked();

  base::Lock lock_;
  bool is_destroyed_;
  std::queue<BufferEntry> buffer_queue_;
  ReadCB read_cb_;

  media::AudioDecoderConfig dummy_audio_decoder_config_;
  media::VideoDecoderConfig video_decoder_config_;
  talk_base::RateTracker frame_rate_tracker_;
};

RTCDemuxerStream::RTCDemuxerStream(const gfx::Size& size)
    : is_destroyed_(false),
      video_decoder_config_(media::kCodecVP8,
                            media::VP8PROFILE_MAIN,
                            media::VideoFrame::NATIVE_TEXTURE,
                            size,
                            gfx::Rect(size),
                            size,
                            NULL,
                            0,
                            false) {}

RTCDemuxerStream::~RTCDemuxerStream() { DCHECK(is_destroyed_); }

media::AudioDecoderConfig RTCDemuxerStream::audio_decoder_config() {
  NOTIMPLEMENTED() << "Does not support audio.";
  return dummy_audio_decoder_config_;
}

media::VideoDecoderConfig RTCDemuxerStream::video_decoder_config() {
  base::AutoLock lock(lock_);
  return video_decoder_config_;
}

DemuxerStream::Type RTCDemuxerStream::type() { return DemuxerStream::VIDEO; }

void RTCDemuxerStream::EnableBitstreamConverter() { NOTREACHED(); }

void RTCDemuxerStream::QueueBuffer(scoped_refptr<media::DecoderBuffer> buffer,
                                   const gfx::Size& new_size) {
  base::AutoLock lock(lock_);
  if (is_destroyed_)
    return;
  buffer_queue_.push(BufferEntry(buffer, new_size));
  if (buffer)
    frame_rate_tracker_.Update(1);
  DVLOG(1) << "frame rate received : " << frame_rate_tracker_.units_second();
  RunReadCallback_Locked();
}

void RTCDemuxerStream::Read(const ReadCB& read_cb) {
  base::AutoLock lock(lock_);
  DCHECK(read_cb_.is_null());
  if (is_destroyed_) {
    media::BindToLoop(base::MessageLoopProxy::current(), read_cb)
        .Run(DemuxerStream::kAborted, NULL);
    return;
  }
  read_cb_ = media::BindToLoop(base::MessageLoopProxy::current(), read_cb);
  RunReadCallback_Locked();
}

void RTCDemuxerStream::Destroy() {
  base::AutoLock lock(lock_);
  DCHECK(!is_destroyed_);
  is_destroyed_ = true;
  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kAborted, NULL);
  while (!buffer_queue_.empty())
    buffer_queue_.pop();
}

void RTCDemuxerStream::RunReadCallback_Locked() {
  if (read_cb_.is_null() || buffer_queue_.empty())
    return;

  BufferEntry& front = buffer_queue_.front();
  if (!front.new_size.IsEmpty()) {
    // No VideoFrame actually reaches GL renderer in Google TV case. We just
    // make coded_size == visible_rect == natural_size here.
    video_decoder_config_.Initialize(media::kCodecVP8,
                                     media::VP8PROFILE_MAIN,
                                     media::VideoFrame::NATIVE_TEXTURE,
                                     front.new_size,
                                     gfx::Rect(front.new_size),
                                     front.new_size,
                                     NULL,
                                     0,
                                     false,
                                     false);
    base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kConfigChanged, NULL);
    front.new_size.SetSize(0, 0);
    return;
  }
  base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kOk, front.decoder_buffer);
  buffer_queue_.pop();
}

// RTCVideoDecoderFactoryTv ----------------------------------------------------

RTCVideoDecoderFactoryTv::RTCVideoDecoderFactoryTv() : is_acquired_(false) {}
RTCVideoDecoderFactoryTv::~RTCVideoDecoderFactoryTv() {}

webrtc::VideoDecoder* RTCVideoDecoderFactoryTv::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  base::AutoLock lock(lock_);
  // One decoder at a time!
  if (decoder_)
    return NULL;
  // Only VP8 is supported --- returning NULL will make WebRTC fall back to SW
  // decoder.
  if (type != webrtc::kVideoCodecVP8)
    return NULL;
  decoder_.reset(new RTCVideoDecoderBridgeTv(this));
  return decoder_.get();
}

void RTCVideoDecoderFactoryTv::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(decoder_.get(), decoder);
  decoder_.reset();
}

bool RTCVideoDecoderFactoryTv::AcquireDemuxer() {
  base::AutoLock lock(lock_);
  if (is_acquired_)
    return false;
  is_acquired_ = true;
  return true;
}

void RTCVideoDecoderFactoryTv::ReleaseDemuxer() {
  base::AutoLock lock(lock_);
  DCHECK(is_acquired_);
  is_acquired_ = false;
  // Clean up internal state as a demuxer.
  init_cb_.Reset();
  if (stream_) {
    stream_->Destroy();
    stream_.reset();
  }
}

void RTCVideoDecoderFactoryTv::Initialize(media::DemuxerHost*,
                                          const media::PipelineStatusCB& cb) {
  base::AutoLock lock(lock_);
  init_cb_ = media::BindToLoop(base::MessageLoopProxy::current(), cb);
  if (stream_)
    base::ResetAndReturn(&init_cb_).Run(media::PIPELINE_OK);
}

DemuxerStream* RTCVideoDecoderFactoryTv::GetStream(DemuxerStream::Type type) {
  base::AutoLock lock(lock_);
  if (type == DemuxerStream::VIDEO)
    return stream_.get();
  return NULL;
}

base::TimeDelta RTCVideoDecoderFactoryTv::GetStartTime() const {
  return base::TimeDelta();
}

void RTCVideoDecoderFactoryTv::InitializeStream(const gfx::Size& size) {
  base::AutoLock lock(lock_);
  DCHECK(!stream_);
  stream_.reset(new RTCDemuxerStream(size));
  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(media::PIPELINE_OK);
}

void RTCVideoDecoderFactoryTv::QueueBuffer(
    scoped_refptr<media::DecoderBuffer> buffer,
    const gfx::Size& new_size) {
  base::AutoLock lock(lock_);
  DCHECK(stream_);
  stream_->QueueBuffer(buffer, new_size);
}

}  // namespace content
