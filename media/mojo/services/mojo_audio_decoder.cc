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

MojoAudioDecoder::MojoAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    interfaces::AudioDecoderPtr remote_decoder)
    : task_runner_(task_runner), remote_decoder_(std::move(remote_decoder)) {
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
  DCHECK(task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();

  // Fail initialization while not implemented.
  task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
}

void MojoAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();

  // Actually we can't decode anything.
  task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, kDecodeError));
}

void MojoAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
}

bool MojoAudioDecoder::NeedsBitstreamConversion() const {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
