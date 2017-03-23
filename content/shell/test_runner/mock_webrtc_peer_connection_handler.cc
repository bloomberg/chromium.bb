// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_webrtc_peer_connection_handler.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/shell/test_runner/mock_webrtc_data_channel_handler.h"
#include "content/shell/test_runner/mock_webrtc_dtmf_sender_handler.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_test_delegate.h"
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

namespace {

class MockWebRTCLegacyStats : public blink::WebRTCLegacyStats {
 public:
  class MemberIterator : public blink::WebRTCLegacyStatsMemberIterator {
   public:
    MemberIterator(
        const std::vector<std::pair<std::string, std::string>>* values)
        : values_(values) {}

    // blink::WebRTCLegacyStatsMemberIterator
    bool isEnd() const override { return i >= values_->size(); }
    void next() override { ++i; }
    blink::WebString name() const override {
      return blink::WebString::fromUTF8((*values_)[i].first);
    }
    blink::WebRTCLegacyStatsMemberType type() const override {
      return blink::WebRTCLegacyStatsMemberTypeString;
    }
    int valueInt() const override {
      NOTREACHED();
      return 0;
    }
    int64_t valueInt64() const override {
      NOTREACHED();
      return 0;
    }
    float valueFloat() const override {
      NOTREACHED();
      return 0.0f;
    }
    blink::WebString valueString() const override {
      return blink::WebString::fromUTF8((*values_)[i].second);
    }
    bool valueBool() const override {
      NOTREACHED();
      return false;
    }
    blink::WebString valueToString() const override { return valueString(); }

   private:
    size_t i = 0;
    const std::vector<std::pair<std::string, std::string>>* values_;
  };

  MockWebRTCLegacyStats(const char* id, const char* type_name, double timestamp)
      : id_(id), type_name_(type_name), timestamp_(timestamp) {}

  // blink::WebRTCLegacyStats
  blink::WebString id() const override {
    return blink::WebString::fromUTF8(id_);
  }
  blink::WebString type() const override {
    return blink::WebString::fromUTF8(type_name_);
  }
  double timestamp() const override { return timestamp_; }
  blink::WebRTCLegacyStatsMemberIterator* iterator() const override {
    return new MemberIterator(&values_);
  }

  void addStatistic(const std::string& name, const std::string& value) {
    values_.push_back(std::make_pair(name, value));
  }

 private:
  const std::string id_;
  const std::string type_name_;
  const double timestamp_;
  // (name, value) pairs.
  std::vector<std::pair<std::string, std::string>> values_;
};

template <typename T>
WebVector<T> sequenceWithValue(T value) {
  return WebVector<T>(&value, 1);
}

class MockWebRTCStatsMember : public blink::WebRTCStatsMember {
 public:
  MockWebRTCStatsMember(const std::string& name,
                        blink::WebRTCStatsMemberType type)
      : name_(name), type_(type) {}

  // blink::WebRTCStatsMember overrides.
  blink::WebString name() const override {
    return blink::WebString::fromUTF8(name_);
  }
  blink::WebRTCStatsMemberType type() const override { return type_; }
  bool isDefined() const override { return true; }
  bool valueBool() const override { return true; }
  int32_t valueInt32() const override { return 42; }
  uint32_t valueUint32() const override { return 42; }
  int64_t valueInt64() const override { return 42; }
  uint64_t valueUint64() const override { return 42; }
  double valueDouble() const override { return 42.0; }
  blink::WebString valueString() const override {
    return blink::WebString::fromUTF8("42");
  }
  WebVector<int> valueSequenceBool() const override {
    return sequenceWithValue<int>(1);
  }
  WebVector<int32_t> valueSequenceInt32() const override {
    return sequenceWithValue<int32_t>(42);
  }
  WebVector<uint32_t> valueSequenceUint32() const override {
    return sequenceWithValue<uint32_t>(42);
  }
  WebVector<int64_t> valueSequenceInt64() const override {
    return sequenceWithValue<int64_t>(42);
  }
  WebVector<uint64_t> valueSequenceUint64() const override {
    return sequenceWithValue<uint64_t>(42);
  }
  WebVector<double> valueSequenceDouble() const override {
    return sequenceWithValue<double>(42.0);
  }
  blink::WebVector<blink::WebString> valueSequenceString() const override {
    return sequenceWithValue<blink::WebString>(
        blink::WebString::fromUTF8("42"));
  }

 private:
  std::string name_;
  blink::WebRTCStatsMemberType type_;
};

class MockWebRTCStats : public blink::WebRTCStats {
 public:
  MockWebRTCStats(const std::string& id,
                  const std::string& type,
                  double timestamp)
      : id_(id), type_(type), timestamp_(timestamp) {}

  void addMember(const std::string& name, blink::WebRTCStatsMemberType type) {
    members_.push_back(std::make_pair(name, type));
  }

  // blink::WebRTCStats overrides.
  blink::WebString id() const override {
    return blink::WebString::fromUTF8(id_);
  }
  blink::WebString type() const override {
    return blink::WebString::fromUTF8(type_);
  }
  double timestamp() const override { return timestamp_; }
  size_t membersCount() const override { return members_.size(); }
  std::unique_ptr<WebRTCStatsMember> getMember(size_t i) const override {
    return std::unique_ptr<WebRTCStatsMember>(
        new MockWebRTCStatsMember(members_[i].first, members_[i].second));
  }

 private:
  std::string id_;
  std::string type_;
  double timestamp_;
  std::vector<std::pair<std::string, blink::WebRTCStatsMemberType>> members_;
};

class MockWebRTCStatsReport : public blink::WebRTCStatsReport {
 public:
  MockWebRTCStatsReport() : i_(0) {}
  MockWebRTCStatsReport(const MockWebRTCStatsReport& other)
      : stats_(other.stats_), i_(0) {}

  void AddStats(const MockWebRTCStats& stats) { stats_.push_back(stats); }

  // blink::WebRTCStatsReport overrides.
  std::unique_ptr<blink::WebRTCStatsReport> copyHandle() const override {
    // Because this is just a mock, we copy the underlying stats instead of
    // referencing the same stats as the original report.
    return std::unique_ptr<blink::WebRTCStatsReport>(
        new MockWebRTCStatsReport(*this));
  }
  std::unique_ptr<WebRTCStats> getStats(WebString id) const override {
    for (const MockWebRTCStats& stats : stats_) {
      if (stats.id() == id)
        return std::unique_ptr<blink::WebRTCStats>(new MockWebRTCStats(stats));
    }
    return nullptr;
  }
  std::unique_ptr<blink::WebRTCStats> next() override {
    if (i_ >= stats_.size())
      return nullptr;
    return std::unique_ptr<blink::WebRTCStats>(
        new MockWebRTCStats(stats_[i_++]));
  }

 private:
  std::vector<MockWebRTCStats> stats_;
  size_t i_;
};

}  // namespace

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler()
    : weak_factory_(this) {}

MockWebRTCPeerConnectionHandler::~MockWebRTCPeerConnectionHandler() {}

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
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&MockWebRTCPeerConnectionHandler::ReportInitializeCompleted,
                 weak_factory_.GetWeakPtr()));
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
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&WebRTCSessionDescriptionRequest::requestSucceeded,
                 base::Owned(new WebRTCSessionDescriptionRequest(request)),
                 session_description));
}

void MockWebRTCPeerConnectionHandler::PostRequestFailure(
    const WebRTCSessionDescriptionRequest& request) {
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&WebRTCSessionDescriptionRequest::requestFailed,
                 base::Owned(new WebRTCSessionDescriptionRequest(request)),
                 WebString("TEST_ERROR")));
}

void MockWebRTCPeerConnectionHandler::PostRequestResult(
    const WebRTCVoidRequest& request) {
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&WebRTCVoidRequest::requestSucceeded,
                 base::Owned(new WebRTCVoidRequest(request))));
}

void MockWebRTCPeerConnectionHandler::PostRequestFailure(
    const WebRTCVoidRequest& request) {
  interfaces_->GetDelegate()->PostTask(base::Bind(
      &WebRTCVoidRequest::requestFailed,
      base::Owned(new WebRTCVoidRequest(request)), WebString("TEST_ERROR")));
}

void MockWebRTCPeerConnectionHandler::createOffer(
    const WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCOfferOptions& options) {
  if (options.iceRestart() && options.voiceActivityDetection() &&
      options.offerToReceiveAudio() > 0 && options.offerToReceiveVideo() > 0) {
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
    blink::WebVector<blink::WebMediaStreamTrack> remote_audio_tracks(
        local_audio_tracks.size());

    for (size_t i = 0; i < local_audio_tracks.size(); ++i) {
      blink::WebMediaStreamSource webkit_source;
      webkit_source.initialize(local_audio_tracks[i].id(),
                               blink::WebMediaStreamSource::TypeAudio,
                               local_audio_tracks[i].id(), true /* remote */);
      remote_audio_tracks[i].initialize(webkit_source);
    }

    blink::WebVector<blink::WebMediaStreamTrack> local_video_tracks;
    stream.videoTracks(local_video_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> remote_video_tracks(
        local_video_tracks.size());
    for (size_t i = 0; i < local_video_tracks.size(); ++i) {
      blink::WebMediaStreamSource webkit_source;
      webkit_source.initialize(local_video_tracks[i].id(),
                               blink::WebMediaStreamSource::TypeVideo,
                               local_video_tracks[i].id(), true /* remote */);
      remote_video_tracks[i].initialize(webkit_source);
    }

    blink::WebMediaStream new_remote_stream;
    new_remote_stream.initialize(remote_audio_tracks, remote_video_tracks);
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

WebRTCErrorType MockWebRTCPeerConnectionHandler::setConfiguration(
    const WebRTCConfiguration& configuration) {
  return WebRTCErrorType::kNone;
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
    MockWebRTCLegacyStats stats("Mock video", "ssrc", current_date);
    stats.addStatistic("type", "video");
    response.addStats(stats);
  } else {
    for (int i = 0; i < stream_count_; ++i) {
      MockWebRTCLegacyStats audio_stats("Mock audio", "ssrc", current_date);
      audio_stats.addStatistic("type", "audio");
      response.addStats(audio_stats);

      MockWebRTCLegacyStats video_stats("Mock video", "ssrc", current_date);
      video_stats.addStatistic("type", "video");
      response.addStats(video_stats);
    }
  }
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&blink::WebRTCStatsRequest::requestSucceeded,
                 base::Owned(new WebRTCStatsRequest(request)), response));
}

void MockWebRTCPeerConnectionHandler::getStats(
    std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
  std::unique_ptr<MockWebRTCStatsReport> report(new MockWebRTCStatsReport());
  MockWebRTCStats stats("mock-stats-01", "mock-stats", 1234.0);
  stats.addMember("bool", blink::WebRTCStatsMemberTypeBool);
  stats.addMember("int32", blink::WebRTCStatsMemberTypeInt32);
  stats.addMember("uint32", blink::WebRTCStatsMemberTypeUint32);
  stats.addMember("int64", blink::WebRTCStatsMemberTypeInt64);
  stats.addMember("uint64", blink::WebRTCStatsMemberTypeUint64);
  stats.addMember("double", blink::WebRTCStatsMemberTypeDouble);
  stats.addMember("string", blink::WebRTCStatsMemberTypeString);
  stats.addMember("sequenceBool", blink::WebRTCStatsMemberTypeSequenceBool);
  stats.addMember("sequenceInt32", blink::WebRTCStatsMemberTypeSequenceInt32);
  stats.addMember("sequenceUint32", blink::WebRTCStatsMemberTypeSequenceUint32);
  stats.addMember("sequenceInt64", blink::WebRTCStatsMemberTypeSequenceInt64);
  stats.addMember("sequenceUint64", blink::WebRTCStatsMemberTypeSequenceUint64);
  stats.addMember("sequenceDouble", blink::WebRTCStatsMemberTypeSequenceDouble);
  stats.addMember("sequenceString", blink::WebRTCStatsMemberTypeSequenceString);
  report->AddStats(stats);
  callback->OnStatsDelivered(
      std::unique_ptr<blink::WebRTCStatsReport>(report.release()));
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
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&MockWebRTCPeerConnectionHandler::ReportCreationOfDataChannel,
                 weak_factory_.GetWeakPtr()));

  // TODO(lukasza): Unclear if it is okay to return a different object than the
  // one created in ReportCreationOfDataChannel.
  return new MockWebRTCDataChannelHandler(label, init,
                                          interfaces_->GetDelegate());
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
