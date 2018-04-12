// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_input_stream_observer.h"

#include <utility>

namespace media {

MojoAudioInputStreamObserver::MojoAudioInputStreamObserver(
    mojom::AudioInputStreamObserverRequest request,
    base::OnceClosure recording_started_callback,
    base::OnceClosure connection_error_callback)
    : binding_(this, std::move(request)),
      recording_started_callback_(std::move(recording_started_callback)) {
  DCHECK(recording_started_callback_);
  binding_.set_connection_error_handler(std::move(connection_error_callback));
}

MojoAudioInputStreamObserver::~MojoAudioInputStreamObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

void MojoAudioInputStreamObserver::DidStartRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(recording_started_callback_);
  std::move(recording_started_callback_).Run();
}

}  // namespace media
