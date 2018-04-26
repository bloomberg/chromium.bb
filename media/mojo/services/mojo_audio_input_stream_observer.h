// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_INPUT_STREAM_OBSERVER_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_INPUT_STREAM_OBSERVER_H_

#include "media/mojo/interfaces/audio_input_stream.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

class UserInputMonitor;

class MEDIA_MOJO_EXPORT MojoAudioInputStreamObserver
    : public mojom::AudioInputStreamObserver {
 public:
  MojoAudioInputStreamObserver(mojom::AudioInputStreamObserverRequest request,
                               base::OnceClosure recording_started_callback,
                               base::OnceClosure connection_error_callback,
                               media::UserInputMonitor* user_input_monitor);
  ~MojoAudioInputStreamObserver() override;

  void DidStartRecording() override;

 private:
  void OnConnectionError();

  mojo::Binding<AudioInputStreamObserver> binding_;
  base::OnceClosure recording_started_callback_;
  base::OnceClosure connection_error_callback_;
  media::UserInputMonitor* user_input_monitor_;

  SEQUENCE_CHECKER(owning_sequence_);

  DISALLOW_COPY_AND_ASSIGN(MojoAudioInputStreamObserver);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_INPUT_STREAM_OBSERVER_H_
