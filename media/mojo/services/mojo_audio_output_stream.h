// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "media/audio/audio_output_delegate.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

// This class handles IPC for single audio output stream by delegating method
// calls to its AudioOutputDelegate.
class MEDIA_MOJO_EXPORT MojoAudioOutputStream
    : public mojom::AudioOutputStream,
      public AudioOutputDelegate::EventHandler {
 public:
  using StreamCreatedCallback =
      mojom::AudioOutputStreamProvider::AcquireCallback;
  using CreateDelegateCallback =
      base::OnceCallback<std::unique_ptr<AudioOutputDelegate>(
          AudioOutputDelegate::EventHandler*)>;

  // |create_delegate_callback| is used to obtain an AudioOutputDelegate for the
  // stream in the constructor. |stream_created_callback| is called when the
  // stream has been initialized. |deleter_callback| is called when this class
  // should be removed (stream ended/error). |deleter_callback| is required to
  // destroy |this| synchronously.
  MojoAudioOutputStream(mojom::AudioOutputStreamRequest request,
                        mojom::AudioOutputStreamClientPtr client,
                        CreateDelegateCallback create_delegate_callback,
                        StreamCreatedCallback stream_created_callback,
                        base::OnceClosure deleter_callback);

  ~MojoAudioOutputStream() override;

 private:
  // mojom::AudioOutputStream implementation.
  void Play() override;
  void Pause() override;
  void SetVolume(double volume) override;

  // AudioOutputDelegate::EventHandler implementation.
  void OnStreamCreated(
      int stream_id,
      const base::SharedMemory* shared_memory,
      std::unique_ptr<base::CancelableSyncSocket> foreign_socket) override;
  void OnStreamError(int stream_id) override;

  // Closes connection to client and notifies owner.
  void OnError();

  SEQUENCE_CHECKER(sequence_checker_);

  StreamCreatedCallback stream_created_callback_;
  base::OnceClosure deleter_callback_;
  mojo::Binding<AudioOutputStream> binding_;
  mojom::AudioOutputStreamClientPtr client_;
  std::unique_ptr<AudioOutputDelegate> delegate_;
  base::WeakPtrFactory<MojoAudioOutputStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_H_
