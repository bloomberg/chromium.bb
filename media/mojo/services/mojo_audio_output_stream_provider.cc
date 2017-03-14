// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream_provider.h"

#include <utility>

namespace media {

MojoAudioOutputStreamProvider::MojoAudioOutputStreamProvider(
    mojom::AudioOutputStreamProviderRequest request,
    CreateDelegateCallback create_delegate_callback,
    DeleterCallback deleter_callback)
    : binding_(this, std::move(request)),
      create_delegate_callback_(std::move(create_delegate_callback)),
      deleter_callback_(base::Bind(std::move(deleter_callback), this)) {
  DCHECK(thread_checker_.CalledOnValidThread());
  binding_.set_connection_error_handler(deleter_callback_);
  DCHECK(create_delegate_callback_);
  DCHECK(deleter_callback_);
}

MojoAudioOutputStreamProvider::~MojoAudioOutputStreamProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MojoAudioOutputStreamProvider::Acquire(
    mojom::AudioOutputStreamRequest stream_request,
    const AudioParameters& params,
    const AcquireCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_output_) {
    LOG(ERROR) << "Output acquired twice.";
    binding_.Unbind();
    deleter_callback_.Run();  // deletes |this|.
    return;
  }

  audio_output_.emplace(
      std::move(stream_request),
      base::BindOnce(std::move(create_delegate_callback_), params),
      std::move(callback), deleter_callback_);
}

}  // namespace media
