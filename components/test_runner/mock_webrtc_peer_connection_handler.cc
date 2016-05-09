// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_webrtc_peer_connection_handler.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/test_runner/mock_webrtc_data_channel_handler.h"
#include "components/test_runner/mock_webrtc_dtmf_sender_handler.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCAnswerOptions.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebRTCOfferOptions.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCStatsResponse.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using namespace blink;

namespace test_runner {

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler()
    : weak_factory_(this) {}

MockWebRTCPeerConnectionHandler::~MockWebRTCPeerConnectionHandler() {
}

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client,
    TestInterfaces* interfaces)
    : client_(client),
      stopped_(false),
      stream_count_(0),
      interfaces_(interfaces),
      weak_factory_(this) {}

void MockWebRTCPeerConnectionHandler::ReportInitializeCompleted() {
  client_->didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete);
  client_->didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted);
}

bool MockWebRTCPeerConnectionHandler::initialize(
    const WebRTCConfiguration& configuration,
    const WebMediaConstraints& constraints) {
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(
      base::Bind(&MockWebRTCPeerConnectionHandler::ReportInitializeCompleted,
                 weak_factory_.GetWeakPtr())));
  return true;
}

void MockWebRTCPeerConnectionHandler::createOffer(
    const WebRTCSessionDescriptionRequest& request,
    const WebMediaConstraints& constraints) {
  PostRequestFailure(request);
}

void MockWebRTCPeerConnectionHandler::PostRequestResult(
    const WebRTCSessionDescriptionRequest& request,
    const WebRTCSessionDescription& session_description) {
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(
      base::Bind(&WebRTCSessionDescriptionRequest::requestSucceeded,
                 base::Owned(new WebRTCSessionDescriptionRequest(request)),
                 session_description)));
}

void MockWebRTCPeerConnectionHandler::PostRequestFailure(
    const WebRTCSessionDescriptionRequest& request) {
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(
      base::Bind(&WebRTCSessionDescriptionRequest::requestFailed,
                 base::Owned(new WebRTCSessionDescriptionRequest(request)),
                 WebString("TEST_ERROR"))));
}

void MockWebRTCPeerConnectionHandler::PostRequestResult(
    const WebRTCVoidRequest& request) {
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(
      base::Bind(&WebRTCVoidRequest::requestSucceeded,
                 base::Owned(new WebRTCVoidRequest(request)))));
}

void MockWebRTCPeerConnectionHandler::PostRequestFailure(
    const WebRTCVoidRequest& request) {
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(base::Bind(
      &WebRTCVoidRequest::requestFailed,
      base::Owned(new WebRTCVoidRequest(request)), WebString("TEST_ERROR"))));
}

void MockWebRTCPeerConnectionHandler::createOffer(
    const WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCOfferOptions& options) {
  if (options.iceRestart() && options.voiceActivityDetection()) {
    WebRTCSessionDescription session_description;
    session_description.initialize("offer", "local");
    PostRequestResult(request, session_description);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::createAnswer(
    const WebRTCSessionDescriptionRequest& request,
    const WebMediaConstraints& constraints) {
  if (!remote_description_.isNull()) {
    WebRTCSessionDescription session_description;
    session_description.initialize("answer", "local");
    PostRequestResult(request, session_description);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::createAnswer(
    const WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCAnswerOptions& options) {
  if (options.voiceActivityDetection()) {
    WebRTCSessionDescription session_description;
    session_description.initialize("answer", "local");
    PostRequestResult(request, session_description);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::setLocalDescription(
    const WebRTCVoidRequest& request,
    const WebRTCSessionDescription& local_description) {
  if (!local_description.isNull() && local_description.sdp() == "local") {
    local_description_ = local_description;
    PostRequestResult(request);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::setRemoteDescription(
    const WebRTCVoidRequest& request,
    const WebRTCSessionDescription& remote_description) {

  if (!remote_description.isNull() && remote_description.sdp() == "remote") {
    UpdateRemoteStreams();
    remote_description_ = remote_description;
    PostRequestResult(request);
  } else
    PostRequestFailure(request);
}

void MockWebRTCPeerConnectionHandler::UpdateRemoteStreams() {
  // Find all removed streams.
  // Set the readyState of the remote tracks to ended, remove them from the
  // stream and notify the client.
  StreamMap::iterator removed_it = remote_streams_.begin();
  while (removed_it != remote_streams_.end()) {
    if (local_streams_.find(removed_it->first) != local_streams_.end()) {
      removed_it++;
      continue;
    }

    // The stream have been removed. Loop through all tracks and set the
    // source as ended and remove them from the stream.
    blink::WebMediaStream stream = removed_it->second;
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    stream.audioTracks(audio_tracks);
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      audio_tracks[i].source().setReadyState(
          blink::WebMediaStreamSource::ReadyStateEnded);
      stream.removeTrack(audio_tracks[i]);
    }

    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    stream.videoTracks(video_tracks);
    for (size_t i = 0; i < video_tracks.size(); ++i) {
      video_tracks[i].source().setReadyState(
          blink::WebMediaStreamSource::ReadyStateEnded);
      stream.removeTrack(video_tracks[i]);
    }
    client_->didRemoveRemoteStream(stream);
    remote_streams_.erase(removed_it++);
  }

  // Find all new streams;
  // Create new sources and tracks and notify the client about the new stream.
  StreamMap::iterator added_it = local_streams_.begin();
  while (added_it != local_streams_.end()) {
    if (remote_streams_.find(added_it->first) != remote_streams_.end()) {
      added_it++;
      continue;
    }

    const blink::WebMediaStream& stream = added_it->second;

    blink::WebVector<blink::WebMediaStreamTrack> local_audio_tracks;
    stream.audioTracks(local_audio_tracks);
    blink::WebVector<blink::WebMediaStreamTrack>
        remote_audio_tracks(local_audio_tracks.size());

    for (size_t i = 0; i < local_audio_tracks.size(); ++i) {
      blink::WebMediaStreamSource webkit_source;
      webkit_source.initialize(local_audio_tracks[i].id(),
                               blink::WebMediaStreamSource::TypeAudio,
                               local_audio_tracks[i].id(),
                               true /* remote */);
      remote_audio_tracks[i].initialize(webkit_source);
    }

    blink::WebVector<blink::WebMediaStreamTrack> local_video_tracks;
    stream.videoTracks(local_video_tracks);
    blink::WebVector<blink::WebMediaStreamTrack>
        remote_video_tracks(local_video_tracks.size());
    for (size_t i = 0; i < local_video_tracks.size(); ++i) {
      blink::WebMediaStreamSource webkit_source;
      webkit_source.initialize(local_video_tracks[i].id(),
                               blink::WebMediaStreamSource::TypeVideo,
                               local_video_tracks[i].id(),
                               true /* remote */);
      remote_video_tracks[i].initialize(webkit_source);
    }

    blink::WebMediaStream new_remote_stream;
    new_remote_stream.initialize(remote_audio_tracks,
                                 remote_video_tracks);
    remote_streams_[added_it->first] = new_remote_stream;
    client_->didAddRemoteStream(new_remote_stream);
    ++added_it;
  }
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::localDescription() {
  return local_description_;
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::remoteDescription() {
  return remote_description_;
}

bool MockWebRTCPeerConnectionHandler::updateICE(
    const WebRTCConfiguration& configuration) {
  return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(
    const WebRTCICECandidate& ice_candidate) {
  client_->didGenerateICECandidate(ice_candidate);
  return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(
    const WebRTCVoidRequest& request,
    const WebRTCICECandidate& ice_candidate) {
  PostRequestResult(request);
  return true;
}

bool MockWebRTCPeerConnectionHandler::addStream(
    const WebMediaStream& stream,
    const WebMediaConstraints& constraints) {
  if (local_streams_.find(stream.id().utf8()) != local_streams_.end())
    return false;
  ++stream_count_;
  client_->negotiationNeeded();
  local_streams_[stream.id().utf8()] = stream;
  return true;
}

void MockWebRTCPeerConnectionHandler::removeStream(
    const WebMediaStream& stream) {
  --stream_count_;
  local_streams_.erase(stream.id().utf8());
  client_->negotiationNeeded();
}

void MockWebRTCPeerConnectionHandler::getStats(
    const WebRTCStatsRequest& request) {
  WebRTCStatsResponse response = request.createResponse();
  double current_date =
      interfaces_->GetDelegate()->GetCurrentTimeInMillisecond();
  if (request.hasSelector()) {
    // FIXME: There is no check that the fetched values are valid.
    size_t report_index =
        response.addReport("Mock video", "ssrc", current_date);
    response.addStatistic(report_index, "type", "video");
  } else {
    for (int i = 0; i < stream_count_; ++i) {
      size_t report_index =
          response.addReport("Mock audio", "ssrc", current_date);
      response.addStatistic(report_index, "type", "audio");
      report_index = response.addReport("Mock video", "ssrc", current_date);
      response.addStatistic(report_index, "type", "video");
    }
  }
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(
      base::Bind(&blink::WebRTCStatsRequest::requestSucceeded,
                 base::Owned(new WebRTCStatsRequest(request)), response)));
}

void MockWebRTCPeerConnectionHandler::ReportCreationOfDataChannel() {
  WebRTCDataChannelInit init;
  WebRTCDataChannelHandler* remote_data_channel =
      new MockWebRTCDataChannelHandler("MockRemoteDataChannel", init,
                                       interfaces_->GetDelegate());
  client_->didAddRemoteDataChannel(remote_data_channel);
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::createDataChannel(
    const WebString& label,
    const blink::WebRTCDataChannelInit& init) {
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(
      base::Bind(&MockWebRTCPeerConnectionHandler::ReportCreationOfDataChannel,
                 weak_factory_.GetWeakPtr())));

  // TODO(lukasza): Unclear if it is okay to return a different object than the
  // one created in ReportCreationOfDataChannel.
  return new MockWebRTCDataChannelHandler(
      label, init, interfaces_->GetDelegate());
}

WebRTCDTMFSenderHandler* MockWebRTCPeerConnectionHandler::createDTMFSender(
    const WebMediaStreamTrack& track) {
  return new MockWebRTCDTMFSenderHandler(track, interfaces_->GetDelegate());
}

void MockWebRTCPeerConnectionHandler::stop() {
  stopped_ = true;
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace test_runner
