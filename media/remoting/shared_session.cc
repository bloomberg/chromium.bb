// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/shared_session.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/remoting/proto_utils.h"

namespace media {
namespace remoting {

SharedSession::SharedSession(mojom::RemotingSourceRequest source_request,
                             mojom::RemoterPtr remoter)
    : rpc_broker_(base::Bind(&SharedSession::SendMessageToSink,
                             base::Unretained(this))),
      binding_(this, std::move(source_request)),
      remoter_(std::move(remoter)) {
  DCHECK(remoter_);
}

SharedSession::~SharedSession() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!clients_.empty()) {
    Shutdown();
    clients_.clear();
  }
}

void SharedSession::OnSinkAvailable(
    mojom::RemotingSinkCapabilities capabilities) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (capabilities == mojom::RemotingSinkCapabilities::NONE) {
    OnSinkGone();
    return;
  }
  sink_capabilities_ = capabilities;
  if (state_ == SESSION_UNAVAILABLE)
    UpdateAndNotifyState(SESSION_CAN_START);
}

void SharedSession::OnSinkGone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  sink_capabilities_ = mojom::RemotingSinkCapabilities::NONE;

  if (state_ == SESSION_PERMANENTLY_STOPPED)
    return;
  if (state_ == SESSION_CAN_START) {
    UpdateAndNotifyState(SESSION_UNAVAILABLE);
    return;
  }
  if (state_ == SESSION_STARTED || state_ == SESSION_STARTING) {
    VLOG(1) << "Sink is gone in a remoting session.";
    // Remoting is being stopped by Remoter.
    UpdateAndNotifyState(SESSION_STOPPING);
  }
}

void SharedSession::OnStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting started successively.";
  if (clients_.empty() || state_ == SESSION_PERMANENTLY_STOPPED ||
      state_ == SESSION_STOPPING) {
    for (Client* client : clients_)
      client->OnStarted(false);
    return;
  }
  for (Client* client : clients_)
    client->OnStarted(true);
  state_ = SESSION_STARTED;
}

void SharedSession::OnStartFailed(mojom::RemotingStartFailReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Failed to start remoting:" << reason;
  for (Client* client : clients_)
    client->OnStarted(false);
  if (state_ == SESSION_PERMANENTLY_STOPPED)
    return;
  state_ = SESSION_UNAVAILABLE;
}

void SharedSession::OnStopped(mojom::RemotingStopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting stopped: " << reason;
  if (state_ == SESSION_PERMANENTLY_STOPPED)
    return;
  UpdateAndNotifyState(SESSION_UNAVAILABLE);
}

void SharedSession::OnMessageFromSink(const std::vector<uint8_t>& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  if (!rpc->ParseFromArray(message.data(), message.size())) {
    VLOG(1) << "corrupted Rpc message";
    Shutdown();
    return;
  }
  rpc_broker_.ProcessMessageFromRemote(std::move(rpc));
}

void SharedSession::UpdateAndNotifyState(SessionState state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == state)
    return;
  state_ = state;
  for (Client* client : clients_)
    client->OnSessionStateChanged();
}

void SharedSession::StartRemoting(Client* client) {
  DCHECK(std::find(clients_.begin(), clients_.end(), client) != clients_.end());

  switch (state_) {
    case SESSION_CAN_START:
      remoter_->Start();
      UpdateAndNotifyState(SESSION_STARTING);
      break;
    case SESSION_STARTING:
      break;
    case SESSION_STARTED:
      client->OnStarted(true);
      break;
    case SESSION_STOPPING:
    case SESSION_UNAVAILABLE:
    case SESSION_PERMANENTLY_STOPPED:
      client->OnStarted(false);
      break;
  }
}

void SharedSession::StopRemoting(Client* client) {
  DCHECK(std::find(clients_.begin(), clients_.end(), client) != clients_.end());

  VLOG(1) << "SharedSession::StopRemoting: " << state_;

  if (state_ != SESSION_STARTING && state_ != SESSION_STARTED)
    return;

  remoter_->Stop(mojom::RemotingStopReason::LOCAL_PLAYBACK);
  UpdateAndNotifyState(SESSION_STOPPING);
}

void SharedSession::AddClient(Client* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find(clients_.begin(), clients_.end(), client) == clients_.end());

  clients_.push_back(client);
}

void SharedSession::RemoveClient(Client* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = std::find(clients_.begin(), clients_.end(), client);
  DCHECK(it != clients_.end());

  clients_.erase(it);
  if (clients_.empty() &&
      (state_ == SESSION_STARTED || state_ == SESSION_STARTING)) {
    remoter_->Stop(mojom::RemotingStopReason::SOURCE_GONE);
    state_ = SESSION_STOPPING;
  }
}

void SharedSession::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == SESSION_STARTED || state_ == SESSION_STARTING)
    remoter_->Stop(mojom::RemotingStopReason::UNEXPECTED_FAILURE);
  UpdateAndNotifyState(SESSION_PERMANENTLY_STOPPED);
}

void SharedSession::StartDataPipe(
    std::unique_ptr<mojo::DataPipe> audio_data_pipe,
    std::unique_ptr<mojo::DataPipe> video_data_pipe,
    const DataPipeStartCallback& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!done_callback.is_null());

  bool audio = audio_data_pipe != nullptr;
  bool video = video_data_pipe != nullptr;
  if (!audio && !video) {
    LOG(ERROR) << "No audio nor video to establish data pipe";
    done_callback.Run(mojom::RemotingDataStreamSenderPtrInfo(),
                      mojom::RemotingDataStreamSenderPtrInfo(),
                      mojo::ScopedDataPipeProducerHandle(),
                      mojo::ScopedDataPipeProducerHandle());
    return;
  }
  mojom::RemotingDataStreamSenderPtr audio_stream_sender;
  mojom::RemotingDataStreamSenderPtr video_stream_sender;
  remoter_->StartDataStreams(audio ? std::move(audio_data_pipe->consumer_handle)
                                   : mojo::ScopedDataPipeConsumerHandle(),
                             video ? std::move(video_data_pipe->consumer_handle)
                                   : mojo::ScopedDataPipeConsumerHandle(),
                             audio ? mojo::MakeRequest(&audio_stream_sender)
                                   : mojom::RemotingDataStreamSenderRequest(),
                             video ? mojo::MakeRequest(&video_stream_sender)
                                   : mojom::RemotingDataStreamSenderRequest());
  done_callback.Run(audio_stream_sender.PassInterface(),
                    video_stream_sender.PassInterface(),
                    audio ? std::move(audio_data_pipe->producer_handle)
                          : mojo::ScopedDataPipeProducerHandle(),
                    video ? std::move(video_data_pipe->producer_handle)
                          : mojo::ScopedDataPipeProducerHandle());
}

void SharedSession::SendMessageToSink(
    std::unique_ptr<std::vector<uint8_t>> message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  remoter_->SendMessageToSink(*message);
}

}  // namespace remoting
}  // namespace media
