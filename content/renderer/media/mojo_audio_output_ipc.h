// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOJO_AUDIO_OUTPUT_IPC_H_
#define CONTENT_RENDERER_MEDIA_MOJO_AUDIO_OUTPUT_IPC_H_

#include <string>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "media/audio/audio_output_ipc.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

// MojoAudioOutputIPC is a renderer-side class for handling creation,
// initialization and control of an output stream. May only be used on a single
// thread.
class CONTENT_EXPORT MojoAudioOutputIPC
    : public media::AudioOutputIPC,
      public media::mojom::AudioOutputStreamClient {
 public:
  using FactoryAccessorCB =
      base::RepeatingCallback<mojom::RendererAudioOutputStreamFactory*()>;

  // |factory_accessor| is required to provide a
  // RendererAudioOutputStreamFactory* if IPC is possible.
  explicit MojoAudioOutputIPC(FactoryAccessorCB factory_accessor);

  ~MojoAudioOutputIPC() override;

  // AudioOutputIPC implementation.
  void RequestDeviceAuthorization(media::AudioOutputIPCDelegate* delegate,
                                  int session_id,
                                  const std::string& device_id,
                                  const url::Origin& security_origin) override;
  void CreateStream(media::AudioOutputIPCDelegate* delegate,
                    const media::AudioParameters& params) override;
  void PlayStream() override;
  void PauseStream() override;
  void CloseStream() override;
  void SetVolume(double volume) override;

  // media::mojom::AudioOutputStreamClient implementation.
  void OnError() override;

 private:
  using AuthorizationCB = mojom::RendererAudioOutputStreamFactory::
      RequestDeviceAuthorizationCallback;

  bool AuthorizationRequested();
  bool StreamCreationRequested();
  media::mojom::AudioOutputStreamProviderRequest MakeProviderRequest();

  // Tries to acquire a RendererAudioOutputStreamFactory, returns true on
  // success. On failure, |this| has been deleted, so returning immediately
  // is required.
  bool DoRequestDeviceAuthorization(int session_id,
                                    const std::string& device_id,
                                    AuthorizationCB callback);

  void ReceivedDeviceAuthorization(media::OutputDeviceStatus status,
                                   const media::AudioParameters& params,
                                   const std::string& device_id) const;

  void StreamCreated(mojo::ScopedSharedBufferHandle shared_memory,
                     mojo::ScopedHandle socket);

  const FactoryAccessorCB factory_accessor_;

  THREAD_CHECKER(thread_checker_);

  mojo::Binding<media::mojom::AudioOutputStreamClient> binding_;
  media::mojom::AudioOutputStreamProviderPtr stream_provider_;
  media::mojom::AudioOutputStreamPtr stream_;
  media::AudioOutputIPCDelegate* delegate_ = nullptr;

  // To make sure we don't send an "authorization completed" callback for a
  // stream after it's closed, we use this weak factory.
  base::WeakPtrFactory<MojoAudioOutputIPC> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioOutputIPC);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOJO_AUDIO_OUTPUT_IPC_H_
