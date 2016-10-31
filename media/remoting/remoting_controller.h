// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CONTROLLER_H_
#define MEDIA_REMOTING_REMOTING_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_observer.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SingleThreadTaskRunner;
}

// This class does the following:
// 1) Sends/Receives messages from/to Remoter;
// 2) Monitors player events as a MediaObserver;
// 3) May trigger the switch of the media renderer between local playback
// and remoting.
//
namespace media {

namespace remoting {
class RpcBroker;
}

class RemotingController final : public MediaObserver,
                                 public mojom::RemotingSource {
 public:
  RemotingController(mojom::RemotingSourceRequest source_request,
                     mojom::RemoterPtr remoter);
  ~RemotingController() override;

  using DataPipeStartCallback =
      base::Callback<void(mojom::RemotingDataStreamSenderPtrInfo audio,
                          mojom::RemotingDataStreamSenderPtrInfo video,
                          mojo::ScopedDataPipeProducerHandle audio_handle,
                          mojo::ScopedDataPipeProducerHandle video_handle)>;
  void StartDataPipe(std::unique_ptr<mojo::DataPipe> audio_data_pipe,
                     std::unique_ptr<mojo::DataPipe> video_data_pipe,
                     const DataPipeStartCallback& done_callback);

  // RemotingSource implementations.
  void OnSinkAvailable() override;
  void OnSinkGone() override;
  void OnStarted() override;
  void OnStartFailed(mojom::RemotingStartFailReason reason) override;
  void OnMessageFromSink(const std::vector<uint8_t>& message) override;
  void OnStopped(mojom::RemotingStopReason reason) override;

  // MediaObserver implementations.
  // This is called when the video element or its ancestor enters full screen.
  // We currently use this as an indicator for immersive playback. May add other
  // criteria (e.g. the actual display width/height of the video element) in
  // future.
  void OnEnteredFullscreen() override;
  void OnExitedFullscreen() override;
  void OnSetCdm(CdmContext* cdm_context) override;
  void OnMetadataChanged(const PipelineMetadata& metadata) override;

  using SwitchRendererCallback = base::Callback<void()>;
  void SetSwitchRendererCallback(const SwitchRendererCallback& cb);

  // Tells which renderer should be used.
  bool is_remoting() const {
    DCHECK(task_runner_->BelongsToCurrentThread());
    return is_remoting_;
  }

  base::WeakPtr<RemotingController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  base::WeakPtr<remoting::RpcBroker> GetRpcBroker() const;

 private:
  bool IsVideoCodecSupported();
  bool IsAudioCodecSupported();

  // Helper to decide whether to enter or leave Remoting mode.
  bool ShouldBeRemoting();

  // Determines whether to enter or leave Remoting mode and switches if
  // necessary.
  void UpdateAndMaybeSwitch();

  // Callback from RpcBroker when sending message to remote sink.
  void OnSendMessageToSink(std::unique_ptr<std::vector<uint8_t>> message);

  // Handle incomging and outgoing RPC message.
  std::unique_ptr<remoting::RpcBroker> rpc_broker_;

  // Indicates if this media element or its ancestor enters full screen.
  bool is_fullscreen_ = false;

  // Indicates the remoting sink availablity.
  bool is_sink_available_ = false;

  // Indicates if remoting is started.
  bool is_remoting_ = false;

  // Indicates whether audio or video is encrypted.
  bool is_encrypted_ = false;

  // Current audio/video config.
  VideoDecoderConfig video_decoder_config_;
  AudioDecoderConfig audio_decoder_config_;
  bool has_audio_ = false;
  bool has_video_ = false;

  // The callback to switch the media renderer.
  SwitchRendererCallback switch_renderer_cb_;

  mojo::Binding<mojom::RemotingSource> binding_;
  mojom::RemoterPtr remoter_;

  // TODO(xjz): Add a media thread task runner for the received RPC messages for
  // remoting media renderer in the up-coming change.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<RemotingController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemotingController);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CONTROLLER_H_
