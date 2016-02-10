// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/android/media_codec_audio_decoder.h"

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"

namespace media {

MediaCodecAudioDecoder::MediaCodecAudioDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      state_(STATE_UNINITIALIZED),
      pending_input_buf_index_(-1),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
}

MediaCodecAudioDecoder::~MediaCodecAudioDecoder() {
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

std::string MediaCodecAudioDecoder::GetDisplayName() const {
  return "MediaCodecAudioDecoder";
}

void MediaCodecAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                        const SetCdmReadyCB& set_cdm_ready_cb,
                                        const InitCB& init_cb,
                                        const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__ << ": " << config.AsHumanReadableString();

  NOTIMPLEMENTED();
}

void MediaCodecAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  NOTIMPLEMENTED();
}

void MediaCodecAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

void MediaCodecAudioDecoder::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

void MediaCodecAudioDecoder::DoIOTask() {
  NOTIMPLEMENTED();
}

bool MediaCodecAudioDecoder::QueueInput() {
  DVLOG(2) << __FUNCTION__;

  NOTIMPLEMENTED();
  return true;
}

bool MediaCodecAudioDecoder::DequeueOutput() {
  DVLOG(2) << __FUNCTION__;

  NOTIMPLEMENTED();
  return true;
}

void MediaCodecAudioDecoder::ManageTimer(bool start) {
  NOTIMPLEMENTED();
}

void MediaCodecAudioDecoder::SetState(State new_state) {
  DVLOG(1) << __FUNCTION__ << ": " << AsString(state_) << "->"
           << AsString(new_state);
  state_ = new_state;
}

scoped_ptr<MediaCodecBridge> MediaCodecAudioDecoder::ConfigureMediaCodec(
    const AudioDecoderConfig& config) {
  NOTIMPLEMENTED();
  return nullptr;
}

void MediaCodecAudioDecoder::OnDecodedFrame(const OutputBufferInfo& out) {
  DVLOG(2) << __FUNCTION__ << " pts:" << out.pts;

  NOTIMPLEMENTED();
}

void MediaCodecAudioDecoder::OnOutputFormatChanged() {
  DVLOG(2) << __FUNCTION__;

  NOTIMPLEMENTED();
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

// static
const char* MediaCodecAudioDecoder::AsString(State state) {
  switch (state) {
    RETURN_STRING(STATE_UNINITIALIZED);
    RETURN_STRING(STATE_READY);
    RETURN_STRING(STATE_WAITING_FOR_KEY);
    RETURN_STRING(STATE_DRAINING);
    RETURN_STRING(STATE_DRAINED);
    RETURN_STRING(STATE_ERROR);
  }
  NOTREACHED() << "Unknown state " << state;
  return nullptr;
}

#undef RETURN_STRING

}  // namespace media
