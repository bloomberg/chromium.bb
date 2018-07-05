// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_set_remote_description_observer.h"

#include "base/logging.h"

namespace content {

WebRtcSetRemoteDescriptionObserver::States::States()
    : signaling_state(
          webrtc::PeerConnectionInterface::SignalingState::kClosed) {}

WebRtcSetRemoteDescriptionObserver::States::States(States&& other)
    : signaling_state(other.signaling_state),
      receiver_states(std::move(other.receiver_states)) {}

WebRtcSetRemoteDescriptionObserver::States::~States() {}

WebRtcSetRemoteDescriptionObserver::States&
WebRtcSetRemoteDescriptionObserver::States::operator=(States&& other) {
  signaling_state = other.signaling_state;
  receiver_states = std::move(other.receiver_states);
  return *this;
}

WebRtcSetRemoteDescriptionObserver::WebRtcSetRemoteDescriptionObserver() {}

WebRtcSetRemoteDescriptionObserver::~WebRtcSetRemoteDescriptionObserver() {}

scoped_refptr<WebRtcSetRemoteDescriptionObserverHandler>
WebRtcSetRemoteDescriptionObserverHandler::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    scoped_refptr<webrtc::PeerConnectionInterface> pc,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    scoped_refptr<WebRtcSetRemoteDescriptionObserver> observer) {
  return new rtc::RefCountedObject<WebRtcSetRemoteDescriptionObserverHandler>(
      std::move(main_task_runner), std::move(signaling_task_runner),
      std::move(pc), std::move(track_adapter_map), std::move(observer));
}

WebRtcSetRemoteDescriptionObserverHandler::
    WebRtcSetRemoteDescriptionObserverHandler(
        scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
        scoped_refptr<webrtc::PeerConnectionInterface> pc,
        scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
        scoped_refptr<WebRtcSetRemoteDescriptionObserver> observer)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      pc_(std::move(pc)),
      track_adapter_map_(std::move(track_adapter_map)),
      observer_(std::move(observer)) {}

WebRtcSetRemoteDescriptionObserverHandler::
    ~WebRtcSetRemoteDescriptionObserverHandler() {}

void WebRtcSetRemoteDescriptionObserverHandler::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error) {
  CHECK(signaling_task_runner_->BelongsToCurrentThread());

  webrtc::RTCErrorOr<WebRtcSetRemoteDescriptionObserver::States>
      states_or_error;
  if (error.ok()) {
    WebRtcSetRemoteDescriptionObserver::States states;
    states.signaling_state = pc_->signaling_state();
    for (const auto& webrtc_receiver : pc_->GetReceivers()) {
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref =
          track_adapter_map_->GetOrCreateRemoteTrackAdapter(
              webrtc_receiver->track().get());
      std::vector<std::string> stream_ids;
      for (const auto& stream : webrtc_receiver->streams())
        stream_ids.push_back(stream->id());
      states.receiver_states.push_back(RtpReceiverState(
          main_task_runner_, signaling_task_runner_, webrtc_receiver.get(),
          std::move(track_ref), std::move(stream_ids)));
    }
    states_or_error = std::move(states);
  } else {
    states_or_error = std::move(error);
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcSetRemoteDescriptionObserverHandler::
                                    OnSetRemoteDescriptionCompleteOnMainThread,
                                this, std::move(states_or_error)));
}

void WebRtcSetRemoteDescriptionObserverHandler::
    OnSetRemoteDescriptionCompleteOnMainThread(
        webrtc::RTCErrorOr<WebRtcSetRemoteDescriptionObserver::States>
            states_or_error) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  if (states_or_error.ok()) {
    for (auto& receiver_state : states_or_error.value().receiver_states)
      receiver_state.Initialize();
  }
  observer_->OnSetRemoteDescriptionComplete(std::move(states_or_error));
}

}  // namespace content
