// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_SHARED_SESSION_H_
#define MEDIA_REMOTING_SHARED_SESSION_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/buildflag.h"
#include "media/media_features.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
#include "media/remoting/rpc_broker.h"  // nogncheck
#endif

namespace media {
namespace remoting {

// A single shared remoting session for multiple clients. The session will
// start remoting when receiving the first request. Once remoting is started,
// it will be stopped when any of the following happens:
// 1) Receives the request from any client to stop remoting.
// 2) Remote sink is gone.
// 3) Any client requests to permanently terminate the session.
// 4) All clients are destroyed.
//
// This class is ref-counted because, in some cases, an instance will have
// shared ownership between RendererController and RemotingCdmController.
class SharedSession final : public mojom::RemotingSource,
                            public base::RefCountedThreadSafe<SharedSession> {
 public:
  // State transition diagram:
  //
  // .--> SESSION_UNAVAILABLE
  // |          |      ^
  // |          V      |
  // |     SESSION_CAN_START
  // |          |
  // |          V
  // | .---SESSION_STARTING --.
  // | |        |             |
  // | |        V             |
  // | |   SESSION_STARTED----|
  // | |        |             |
  // | |        V             |
  // | '-> SESSION_STOPPING   |
  // '-----'    |             |
  //            V             V
  //    SESSION_PERMANENTLY_STOPPED
  enum SessionState {
    // Remoting sink is not available. Can't start remoting.
    SESSION_UNAVAILABLE,
    // Remoting sink is available, Can start remoting.
    SESSION_CAN_START,
    // Starting a remoting session.
    SESSION_STARTING,
    // Remoting session is successively started.
    SESSION_STARTED,
    // Stopping the session.
    SESSION_STOPPING,
    // Remoting session is permanently stopped. This state indicates that the
    // video stack cannot continue operation. For example, if a remoting session
    // involving CDM content was stopped, there is no way to continue playback
    // because the CDM is required but is no longer available.
    SESSION_PERMANENTLY_STOPPED,
  };

  class Client {
   public:
    // Get notified whether the remoting session is successively started.
    virtual void OnStarted(bool success) = 0;
    // Get notified when session state changes.
    virtual void OnSessionStateChanged() = 0;
  };

  SharedSession(mojom::RemotingSourceRequest source_request,
                mojom::RemoterPtr remoter);

  // Get the current session state.
  SessionState state() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return state_;
  }

  // Get the last stop reason reported by OnStopped().
  mojom::RemotingStopReason get_last_stop_reason() const {
    return stop_reason_;
  }

  const std::string& sink_name() const { return sink_metadata_.friendly_name; }

  // Queries on remoting sink capabilities.
  bool HasVideoCapability(mojom::RemotingSinkVideoCapability capability) const;
  bool HasAudioCapability(mojom::RemotingSinkAudioCapability capability) const;
  bool HasFeatureCapability(mojom::RemotingSinkFeature capability) const;
  bool IsRemoteDecryptionAvailable() const;

  // RemotingSource implementations.
  void OnSinkAvailable(mojom::RemotingSinkMetadataPtr metadata) override;
  void OnSinkGone() override;
  void OnStarted() override;
  void OnStartFailed(mojom::RemotingStartFailReason reason) override;
  void OnMessageFromSink(const std::vector<uint8_t>& message) override;
  void OnStopped(mojom::RemotingStopReason reason) override;

  using DataPipeStartCallback =
      base::Callback<void(mojom::RemotingDataStreamSenderPtrInfo audio,
                          mojom::RemotingDataStreamSenderPtrInfo video,
                          mojo::ScopedDataPipeProducerHandle audio_handle,
                          mojo::ScopedDataPipeProducerHandle video_handle)>;
  void StartDataPipe(std::unique_ptr<mojo::DataPipe> audio_data_pipe,
                     std::unique_ptr<mojo::DataPipe> video_data_pipe,
                     const DataPipeStartCallback& done_callback);

  // Requests to start remoting. Will try start a remoting session if not
  // started yet. |client| will get informed whether the session is
  // successifully started throught OnStarted().
  void StartRemoting(Client* client);

  // Requests to stop the current remoting session if started. When the session
  // is stopping, all clients will get notified.
  void StopRemoting(Client* client);

  // Permanently terminates the current remoting session.
  void Shutdown();

  // Add/remove a client to/from |clients_|.
  // Note: Clients can only added/removed through these methods.
  // Remoting session will be stopped if all clients are gone.
  void AddClient(Client* client);
  void RemoveClient(Client* client);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  RpcBroker* rpc_broker() { return &rpc_broker_; }
#endif

  void EstimateTransmissionCapacity(
      mojom::Remoter::EstimateTransmissionCapacityCallback callback);

 private:
  friend class base::RefCountedThreadSafe<SharedSession>;
  ~SharedSession() override;

  // Updates the current session state and notifies all the clients if state
  // changes.
  void UpdateAndNotifyState(SessionState state);

  // Callback from RpcBroker when sending message to remote sink.
  void SendMessageToSink(std::unique_ptr<std::vector<uint8_t>> message);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  // Handles dispatching of incoming and outgoing RPC messages.
  RpcBroker rpc_broker_;
#endif

  const mojo::Binding<mojom::RemotingSource> binding_;
  const mojom::RemoterPtr remoter_;

  // When the sink is available for remoting, this describes its metadata. When
  // not available, this is empty. Updated by OnSinkAvailable/Gone().
  mojom::RemotingSinkMetadata sink_metadata_;

  // The current state.
  SessionState state_ = SESSION_UNAVAILABLE;

  // Clients are added/removed to/from this list by calling Add/RemoveClient().
  // All the clients are not belong to this class. They are supposed to call
  // RemoveClient() before they are gone.
  std::vector<Client*> clients_;

  mojom::RemotingStopReason stop_reason_ =
      mojom::RemotingStopReason::LOCAL_PLAYBACK;

  // This is used to check all the methods are called on the current thread in
  // debug builds.
  base::ThreadChecker thread_checker_;
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_SHARED_SESSION_H_
