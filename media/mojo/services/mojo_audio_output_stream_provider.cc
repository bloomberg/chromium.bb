// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream_provider.h"

#include <utility>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/message.h"

namespace media {

MojoAudioOutputStreamProvider::MojoAudioOutputStreamProvider(
    mojom::AudioOutputStreamProviderRequest request,
    CreateDelegateCallback create_delegate_callback,
    DeleterCallback deleter_callback,
    std::unique_ptr<media::mojom::AudioOutputStreamObserver> observer)
    : binding_(this, std::move(request)),
      create_delegate_callback_(std::move(create_delegate_callback)),
      deleter_callback_(std::move(deleter_callback)),
      observer_(std::move(observer)),
      observer_binding_(observer_.get()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unretained is safe since |this| owns |binding_|.
  binding_.set_connection_error_handler(base::Bind(
      &MojoAudioOutputStreamProvider::OnError, base::Unretained(this)));
  DCHECK(create_delegate_callback_);
  DCHECK(deleter_callback_);
}

MojoAudioOutputStreamProvider::~MojoAudioOutputStreamProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoAudioOutputStreamProvider::Acquire(
    mojom::AudioOutputStreamRequest stream_request,
    mojom::AudioOutputStreamClientPtr client,
    const AudioParameters& params,
    AcquireCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if !defined(OS_ANDROID)
  if (params.IsBitstreamFormat()) {
    // Bitstream streams are only supported on Android.
    BadMessage(
        "Attempted to acquire a bitstream audio stream on a platform where "
        "it's not supported");
    return;
  }
#endif
  if (audio_output_) {
    BadMessage("Output acquired twice.");
    return;
  }

  mojom::AudioOutputStreamObserverPtr observer_ptr;
  observer_binding_.Bind(mojo::MakeRequest(&observer_ptr));
  // Unretained is safe since |this| owns |audio_output_|.
  audio_output_.emplace(std::move(stream_request), std::move(client),
                        base::BindOnce(std::move(create_delegate_callback_),
                                       params, std::move(observer_ptr)),
                        std::move(callback),
                        base::BindOnce(&MojoAudioOutputStreamProvider::OnError,
                                       base::Unretained(this)));
}

void MojoAudioOutputStreamProvider::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deletes |this|:
  std::move(deleter_callback_).Run(this);
}

void MojoAudioOutputStreamProvider::BadMessage(const std::string& error) {
  mojo::ReportBadMessage(error);
  if (binding_.is_bound())
    binding_.Unbind();
  if (observer_binding_.is_bound())
    observer_binding_.Unbind();
  std::move(deleter_callback_).Run(this);  // deletes |this|.
}

}  // namespace media
