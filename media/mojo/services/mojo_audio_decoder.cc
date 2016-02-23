// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace media {

MojoAudioDecoder::MojoAudioDecoder() {
  DVLOG(1) << __FUNCTION__;
}

MojoAudioDecoder::~MojoAudioDecoder() {
  DVLOG(1) << __FUNCTION__;
}

std::string MojoAudioDecoder::GetDisplayName() const {
  return "MojoAudioDecoder";
}

void MojoAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  CdmContext* cdm_context,
                                  const InitCB& init_cb,
                                  const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();

  NOTIMPLEMENTED();

  // Pretend to be able to decode anything.
  task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, true));
}

void MojoAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(3) << __FUNCTION__;
  NOTIMPLEMENTED();

  // Actually we can't decode anything.
  task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, kDecodeError));
}

void MojoAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __FUNCTION__;
  NOTIMPLEMENTED();
}

bool MojoAudioDecoder::NeedsBitstreamConversion() const {
  DVLOG(1) << __FUNCTION__;
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
