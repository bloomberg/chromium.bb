// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_input_stream_observer.h"

#include <utility>

#include "media/base/user_input_monitor.h"

namespace media {

MojoAudioInputStreamObserver::MojoAudioInputStreamObserver(
    mojom::AudioInputStreamObserverRequest request,
    base::OnceClosure recording_started_callback,
    base::OnceClosure connection_error_callback,
    media::UserInputMonitor* user_input_monitor)
    : binding_(this, std::move(request)),
      recording_started_callback_(std::move(recording_started_callback)),
      connection_error_callback_(std::move(connection_error_callback)),
      user_input_monitor_(user_input_monitor) {
  DCHECK(recording_started_callback_);
  if (user_input_monitor_)
    user_input_monitor_->EnableKeyPressMonitoring();

  // Unretained is safe because |this| owns |binding_|.
  binding_.set_connection_error_handler(
      base::BindOnce(&MojoAudioInputStreamObserver::OnConnectionError,
                     base::Unretained(this)));
}

MojoAudioInputStreamObserver::~MojoAudioInputStreamObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  // Check that DisableKeyPressMonitoring() was called.
  DCHECK(!user_input_monitor_);
}

void MojoAudioInputStreamObserver::DidStartRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(recording_started_callback_);
  std::move(recording_started_callback_).Run();
}

void MojoAudioInputStreamObserver::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (user_input_monitor_) {
    user_input_monitor_->DisableKeyPressMonitoring();

    // Set to nullptr to check that DisableKeyPressMonitoring() was called
    // before destructor.
    user_input_monitor_ = nullptr;
  }

  std::move(connection_error_callback_).Run();
}

}  // namespace media
