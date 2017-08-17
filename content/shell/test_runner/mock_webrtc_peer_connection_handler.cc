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
#include "base/memory/ptr_util.h"
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
#include "third_party/WebKit/public/platform/WebRTCRtpContributingSource.h"
#include "third_party/WebKit/public/platform/WebRTCRtpReceiver.h"
#include "third_party/WebKit/public/platform/WebRTCRtpSender.h"
#include "third_party/WebKit/public/platform/WebRTCStatsResponse.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using namespace blink;

namespace test_runner {

namespace {

uintptr_t GetIDByTrack(const std::string& track_id,
                       std::map<std::string, uintptr_t>* id_by_track) {
  const auto& it = id_by_track->find(track_id);
  if (it == id_by_track->end()) {
    uintptr_t id = static_cast<uintptr_t>(id_by_track->size()) + 1;
    id_by_track->insert(std::make_pair(track_id, id));
    return id;
  }
  return it->second;
}

class MockWebRTCLegacyStats : public blink::WebRTCLegacyStats {
 public:
  class MemberIterator : public blink::WebRTCLegacyStatsMemberIterator {
   public:
    MemberIterator(
        const std::vector<std::pair<std::string, std::string>>* values)
        : values_(values) {}

    // blink::WebRTCLegacyStatsMemberIterator
    bool IsEnd() const override { return i >= values_->size(); }
    void Next() override { ++i; }
    blink::WebString GetName() const override {
      return blink::WebString::FromUTF8((*values_)[i].first);
    }
    blink::WebRTCLegacyStatsMemberType GetType() const override {
      return blink::kWebRTCLegacyStatsMemberTypeString;
    }
    int ValueInt() const override {
      NOTREACHED();
      return 0;
    }
    int64_t ValueInt64() const override {
      NOTREACHED();
      return 0;
    }
    float ValueFloat() const override {
      NOTREACHED();
      return 0.0f;
    }
    blink::WebString ValueString() const override {
      return blink::WebString::FromUTF8((*values_)[i].second);
    }
    bool ValueBool() const override {
      NOTREACHED();
      return false;
    }
    blink::WebString ValueToString() const override { return ValueString(); }

   private:
    size_t i = 0;
    const std::vector<std::pair<std::string, std::string>>* values_;
  };

  MockWebRTCLegacyStats(const char* id, const char* type_name, double timestamp)
      : id_(id), type_name_(type_name), timestamp_(timestamp) {}

  // blink::WebRTCLegacyStats
  blink::WebString Id() const override {
    return blink::WebString::FromUTF8(id_);
  }
  blink::WebString GetType() const override {
    return blink::WebString::FromUTF8(type_name_);
  }
  double Timestamp() const override { return timestamp_; }
  blink::WebRTCLegacyStatsMemberIterator* Iterator() const override {
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
  blink::WebString GetName() const override {
    return blink::WebString::FromUTF8(name_);
  }
  blink::WebRTCStatsMemberType GetType() const override { return type_; }
  bool IsDefined() const override { return true; }
  bool ValueBool() const override { return true; }
  int32_t ValueInt32() const override { return 42; }
  uint32_t ValueUint32() const override { return 42; }
  int64_t ValueInt64() const override { return 42; }
  uint64_t ValueUint64() const override { return 42; }
  double ValueDouble() const override { return 42.0; }
  blink::WebString ValueString() const override {
    return blink::WebString::FromUTF8("42");
  }
  WebVector<int> ValueSequenceBool() const override {
    return sequenceWithValue<int>(1);
  }
  WebVector<int32_t> ValueSequenceInt32() const override {
    return sequenceWithValue<int32_t>(42);
  }
  WebVector<uint32_t> ValueSequenceUint32() const override {
    return sequenceWithValue<uint32_t>(42);
  }
  WebVector<int64_t> ValueSequenceInt64() const override {
    return sequenceWithValue<int64_t>(42);
  }
  WebVector<uint64_t> ValueSequenceUint64() const override {
    return sequenceWithValue<uint64_t>(42);
  }
  WebVector<double> ValueSequenceDouble() const override {
    return sequenceWithValue<double>(42.0);
  }
  blink::WebVector<blink::WebString> ValueSequenceString() const override {
    return sequenceWithValue<blink::WebString>(
        blink::WebString::FromUTF8("42"));
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
  blink::WebString Id() const override {
    return blink::WebString::FromUTF8(id_);
  }
  blink::WebString GetType() const override {
    return blink::WebString::FromUTF8(type_);
  }
  double Timestamp() const override { return timestamp_; }
  size_t MembersCount() const override { return members_.size(); }
  std::unique_ptr<WebRTCStatsMember> GetMember(size_t i) const override {
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
  std::unique_ptr<blink::WebRTCStatsReport> CopyHandle() const override {
    // Because this is just a mock, we copy the underlying stats instead of
    // referencing the same stats as the original report.
    return std::unique_ptr<blink::WebRTCStatsReport>(
        new MockWebRTCStatsReport(*this));
  }
  std::unique_ptr<WebRTCStats> GetStats(WebString id) const override {
    for (const MockWebRTCStats& stats : stats_) {
      if (stats.Id() == id)
        return std::unique_ptr<blink::WebRTCStats>(new MockWebRTCStats(stats));
    }
    return nullptr;
  }
  std::unique_ptr<blink::WebRTCStats> Next() override {
    if (i_ >= stats_.size())
      return nullptr;
    return std::unique_ptr<blink::WebRTCStats>(
        new MockWebRTCStats(stats_[i_++]));
  }
  size_t Size() const override { return stats_.size(); }

 private:
  std::vector<MockWebRTCStats> stats_;
  size_t i_;
};

class MockWebRTCRtpSender : public blink::WebRTCRtpSender {
 public:
  MockWebRTCRtpSender(uintptr_t id,
                      std::unique_ptr<blink::WebMediaStreamTrack> track)
      : id_(id), track_(std::move(track)) {}

  uintptr_t Id() const override { return id_; }
  const blink::WebMediaStreamTrack* Track() const override {
    return track_.get();
  }

  void SetTrack(std::unique_ptr<blink::WebMediaStreamTrack> track) {
    track_ = std::move(track);
  }

 private:
  uintptr_t id_;
  std::unique_ptr<blink::WebMediaStreamTrack> track_;
};

class MockWebRTCRtpContributingSource
    : public blink::WebRTCRtpContributingSource {
 public:
  MockWebRTCRtpContributingSource(
      blink::WebRTCRtpContributingSourceType source_type,
      double timestamp_ms,
      uint32_t source)
      : source_type_(source_type),
        timestamp_ms_(timestamp_ms),
        source_(source) {}
  ~MockWebRTCRtpContributingSource() override {}

  blink::WebRTCRtpContributingSourceType SourceType() const override {
    return source_type_;
  }
  double TimestampMs() const override { return timestamp_ms_; }
  uint32_t Source() const override { return source_; }

 private:
  blink::WebRTCRtpContributingSourceType source_type_;
  double timestamp_ms_;
  uint32_t source_;
};

class MockWebRTCRtpReceiver : public blink::WebRTCRtpReceiver {
 public:
  MockWebRTCRtpReceiver(uintptr_t id, const blink::WebMediaStreamTrack& track)
      : id_(id), track_(track), num_packets_(0) {}
  ~MockWebRTCRtpReceiver() override {}

  uintptr_t Id() const override { return id_; }
  const blink::WebMediaStreamTrack& Track() const override { return track_; }

  // Every time called, mocks that a new packet has arrived updating the i-th
  // CSRC such that the |kNumCSRCsActive| latest updated CSRCs are returned. "i"
  // is the sequence number modulo number of CSRCs. Also returns an SSRC with
  // the latest timestamp. For example, if 2 out of 3 CSRCs should be active,
  // this will return the following "(type, source, timestamp)":
  // - 1st call: { (CSRC, 0, 0), (SSRC, 0, 0) }
  // - 2nd call: { (CSRC, 0, 0000), (CSRC, 1, 5000), (SSRC, 0, 5000) }
  // - 3rd call: { (CSRC, 1, 5000), (CSRC, 2, 10000), (SSRC, 0, 10000) }
  // - 4th call: { (CSRC, 2, 10000), (CSRC, 0, 15000), (SSRC, 0, 15000) }
  // RTCPeerConnection-getReceivers.html depends on this behavior.
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
  GetSources() override {
    ++num_packets_;
    size_t num_csrcs = std::min(kNumCSRCsActive, num_packets_);
    blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
        contributing_sources(num_csrcs + 1);
    for (size_t i = 0; i < num_csrcs; ++i) {
      size_t sequence_number = num_packets_ - num_csrcs + i;
      contributing_sources[i] =
          base::MakeUnique<MockWebRTCRtpContributingSource>(
              blink::WebRTCRtpContributingSourceType::CSRC,
              // Return value should include timestamps for the last 10 seconds,
              // we pretend |10000.0 / kNumCSRCsActive| milliseconds have passed
              // per packet in the sequence, starting from 0. This is not
              // relative to any real clock.
              sequence_number * 10000.0 / kNumCSRCsActive,
              sequence_number % kNumCSRCs);
    }
    contributing_sources[num_csrcs] =
        base::MakeUnique<MockWebRTCRtpContributingSource>(
            blink::WebRTCRtpContributingSourceType::SSRC,
            contributing_sources[num_csrcs - 1]->TimestampMs(), 0);
    return contributing_sources;
  }

 private:
  const size_t kNumCSRCs = 3;
  const size_t kNumCSRCsActive = 2;

  uintptr_t id_;
  blink::WebMediaStreamTrack track_;
  size_t num_packets_;
};

blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>>
GetReceiversOfStream(const blink::WebMediaStream& remote_web_stream,
                     std::map<std::string, uintptr_t>* id_by_track) {
  std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>> receivers;
  blink::WebVector<blink::WebMediaStreamTrack> remote_tracks;
  remote_web_stream.AudioTracks(remote_tracks);
  for (const auto& remote_track : remote_tracks) {
    receivers.push_back(
        std::unique_ptr<blink::WebRTCRtpReceiver>(new MockWebRTCRtpReceiver(
            GetIDByTrack(remote_track.Id().Utf8(), id_by_track),
            remote_track)));
  }
  remote_web_stream.VideoTracks(remote_tracks);
  for (const auto& remote_track : remote_tracks) {
    receivers.push_back(
        std::unique_ptr<blink::WebRTCRtpReceiver>(new MockWebRTCRtpReceiver(
            GetIDByTrack(remote_track.Id().Utf8(), id_by_track),
            remote_track)));
  }
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>> result(
      receivers.size());
  for (size_t i = 0; i < receivers.size(); ++i) {
    result[i] = std::move(receivers[i]);
  }
  return result;
}

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
  client_->DidChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::kICEGatheringStateComplete);
  client_->DidChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::kICEConnectionStateCompleted);
}

bool MockWebRTCPeerConnectionHandler::Initialize(
    const WebRTCConfiguration& configuration,
    const WebMediaConstraints& constraints) {
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&MockWebRTCPeerConnectionHandler::ReportInitializeCompleted,
                 weak_factory_.GetWeakPtr()));
  return true;
}

void MockWebRTCPeerConnectionHandler::CreateOffer(
    const WebRTCSessionDescriptionRequest& request,
    const WebMediaConstraints& constraints) {
  PostRequestFailure(request);
}

void MockWebRTCPeerConnectionHandler::PostRequestResult(
    const WebRTCSessionDescriptionRequest& request,
    const WebRTCSessionDescription& session_description) {
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&WebRTCSessionDescriptionRequest::RequestSucceeded,
                 base::Owned(new WebRTCSessionDescriptionRequest(request)),
                 session_description));
}

void MockWebRTCPeerConnectionHandler::PostRequestFailure(
    const WebRTCSessionDescriptionRequest& request) {
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&WebRTCSessionDescriptionRequest::RequestFailed,
                 base::Owned(new WebRTCSessionDescriptionRequest(request)),
                 WebString("TEST_ERROR")));
}

void MockWebRTCPeerConnectionHandler::PostRequestResult(
    const WebRTCVoidRequest& request) {
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&WebRTCVoidRequest::RequestSucceeded,
                 base::Owned(new WebRTCVoidRequest(request))));
}

void MockWebRTCPeerConnectionHandler::PostRequestFailure(
    const WebRTCVoidRequest& request) {
  interfaces_->GetDelegate()->PostTask(base::Bind(
      &WebRTCVoidRequest::RequestFailed,
      base::Owned(new WebRTCVoidRequest(request)), WebString("TEST_ERROR")));
}

void MockWebRTCPeerConnectionHandler::CreateOffer(
    const WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCOfferOptions& options) {
  if (options.IceRestart() && options.VoiceActivityDetection() &&
      options.OfferToReceiveAudio() > 0 && options.OfferToReceiveVideo() > 0) {
    WebRTCSessionDescription session_description;
    session_description.Initialize("offer", "local");
    PostRequestResult(request, session_description);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::CreateAnswer(
    const WebRTCSessionDescriptionRequest& request,
    const WebMediaConstraints& constraints) {
  if (!remote_description_.IsNull()) {
    WebRTCSessionDescription session_description;
    session_description.Initialize("answer", "local");
    PostRequestResult(request, session_description);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::CreateAnswer(
    const WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCAnswerOptions& options) {
  if (options.VoiceActivityDetection()) {
    WebRTCSessionDescription session_description;
    session_description.Initialize("answer", "local");
    PostRequestResult(request, session_description);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::SetLocalDescription(
    const WebRTCVoidRequest& request,
    const WebRTCSessionDescription& local_description) {
  if (!local_description.IsNull() && local_description.Sdp() == "local") {
    local_description_ = local_description;
    PostRequestResult(request);
  } else {
    PostRequestFailure(request);
  }
}

void MockWebRTCPeerConnectionHandler::SetRemoteDescription(
    const WebRTCVoidRequest& request,
    const WebRTCSessionDescription& remote_description) {
  if (!remote_description.IsNull() && remote_description.Sdp() == "remote") {
    remote_description_ = remote_description;
    // This ensures the promise is resolved before the remote stream event is
    // fired. This may change in the future, see
    // https://github.com/w3c/webrtc-pc/issues/1508.
    interfaces_->GetDelegate()->PostTask(
        base::Bind(&MockWebRTCPeerConnectionHandler::UpdateRemoteStreams,
                   weak_factory_.GetWeakPtr()));
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
    stream.AudioTracks(audio_tracks);
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      audio_tracks[i].Source().SetReadyState(
          blink::WebMediaStreamSource::kReadyStateEnded);
      stream.RemoveTrack(audio_tracks[i]);
    }

    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    stream.VideoTracks(video_tracks);
    for (size_t i = 0; i < video_tracks.size(); ++i) {
      video_tracks[i].Source().SetReadyState(
          blink::WebMediaStreamSource::kReadyStateEnded);
      stream.RemoveTrack(video_tracks[i]);
    }
    client_->DidRemoveRemoteStream(stream);
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
    stream.AudioTracks(local_audio_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> remote_audio_tracks(
        local_audio_tracks.size());

    for (size_t i = 0; i < local_audio_tracks.size(); ++i) {
      blink::WebMediaStreamSource webkit_source;
      webkit_source.Initialize(local_audio_tracks[i].Id(),
                               blink::WebMediaStreamSource::kTypeAudio,
                               local_audio_tracks[i].Id(), true /* remote */);
      remote_audio_tracks[i].Initialize(webkit_source);
    }

    blink::WebVector<blink::WebMediaStreamTrack> local_video_tracks;
    stream.VideoTracks(local_video_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> remote_video_tracks(
        local_video_tracks.size());
    for (size_t i = 0; i < local_video_tracks.size(); ++i) {
      blink::WebMediaStreamSource webkit_source;
      webkit_source.Initialize(local_video_tracks[i].Id(),
                               blink::WebMediaStreamSource::kTypeVideo,
                               local_video_tracks[i].Id(), true /* remote */);
      remote_video_tracks[i].Initialize(webkit_source);
    }

    blink::WebMediaStream new_remote_stream;
    new_remote_stream.Initialize(remote_audio_tracks, remote_video_tracks);
    remote_streams_[added_it->first] = new_remote_stream;
    blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>> receivers =
        GetReceiversOfStream(new_remote_stream, &id_by_track_);
    client_->DidAddRemoteStream(new_remote_stream, &receivers);
    ++added_it;
  }
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::LocalDescription() {
  return local_description_;
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::RemoteDescription() {
  return remote_description_;
}

WebRTCErrorType MockWebRTCPeerConnectionHandler::SetConfiguration(
    const WebRTCConfiguration& configuration) {
  return WebRTCErrorType::kNone;
}

bool MockWebRTCPeerConnectionHandler::AddICECandidate(
    const WebRTCICECandidate& ice_candidate) {
  client_->DidGenerateICECandidate(ice_candidate);
  return true;
}

bool MockWebRTCPeerConnectionHandler::AddICECandidate(
    const WebRTCVoidRequest& request,
    const WebRTCICECandidate& ice_candidate) {
  PostRequestResult(request);
  return true;
}

bool MockWebRTCPeerConnectionHandler::AddStream(
    const WebMediaStream& stream,
    const WebMediaConstraints& constraints) {
  if (local_streams_.find(stream.Id().Utf8()) != local_streams_.end())
    return false;
  ++stream_count_;
  client_->NegotiationNeeded();
  local_streams_[stream.Id().Utf8()] = stream;
  return true;
}

void MockWebRTCPeerConnectionHandler::RemoveStream(
    const WebMediaStream& stream) {
  --stream_count_;
  local_streams_.erase(stream.Id().Utf8());
  client_->NegotiationNeeded();
}

void MockWebRTCPeerConnectionHandler::GetStats(
    const WebRTCStatsRequest& request) {
  WebRTCStatsResponse response = request.CreateResponse();
  double current_date =
      interfaces_->GetDelegate()->GetCurrentTimeInMillisecond();
  if (request.HasSelector()) {
    // FIXME: There is no check that the fetched values are valid.
    MockWebRTCLegacyStats stats("Mock video", "ssrc", current_date);
    stats.addStatistic("type", "video");
    response.AddStats(stats);
  } else {
    for (int i = 0; i < stream_count_; ++i) {
      MockWebRTCLegacyStats audio_stats("Mock audio", "ssrc", current_date);
      audio_stats.addStatistic("type", "audio");
      response.AddStats(audio_stats);

      MockWebRTCLegacyStats video_stats("Mock video", "ssrc", current_date);
      video_stats.addStatistic("type", "video");
      response.AddStats(video_stats);
    }
  }
  interfaces_->GetDelegate()->PostTask(
      base::Bind(&blink::WebRTCStatsRequest::RequestSucceeded,
                 base::Owned(new WebRTCStatsRequest(request)), response));
}

void MockWebRTCPeerConnectionHandler::GetStats(
    std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
  std::unique_ptr<MockWebRTCStatsReport> report(new MockWebRTCStatsReport());
  MockWebRTCStats stats("mock-stats-01", "mock-stats", 1234.0);
  stats.addMember("bool", blink::kWebRTCStatsMemberTypeBool);
  stats.addMember("int32", blink::kWebRTCStatsMemberTypeInt32);
  stats.addMember("uint32", blink::kWebRTCStatsMemberTypeUint32);
  stats.addMember("int64", blink::kWebRTCStatsMemberTypeInt64);
  stats.addMember("uint64", blink::kWebRTCStatsMemberTypeUint64);
  stats.addMember("double", blink::kWebRTCStatsMemberTypeDouble);
  stats.addMember("string", blink::kWebRTCStatsMemberTypeString);
  stats.addMember("sequenceBool", blink::kWebRTCStatsMemberTypeSequenceBool);
  stats.addMember("sequenceInt32", blink::kWebRTCStatsMemberTypeSequenceInt32);
  stats.addMember("sequenceUint32",
                  blink::kWebRTCStatsMemberTypeSequenceUint32);
  stats.addMember("sequenceInt64", blink::kWebRTCStatsMemberTypeSequenceInt64);
  stats.addMember("sequenceUint64",
                  blink::kWebRTCStatsMemberTypeSequenceUint64);
  stats.addMember("sequenceDouble",
                  blink::kWebRTCStatsMemberTypeSequenceDouble);
  stats.addMember("sequenceString",
                  blink::kWebRTCStatsMemberTypeSequenceString);
  report->AddStats(stats);
  callback->OnStatsDelivered(
      std::unique_ptr<blink::WebRTCStatsReport>(report.release()));
}

blink::WebVector<std::unique_ptr<blink::WebRTCRtpSender>>
MockWebRTCPeerConnectionHandler::GetSenders() {
  std::vector<std::unique_ptr<blink::WebRTCRtpSender>> senders;
  // Senders of tracks in |local_streams_| (from |Add/RemoveStream|).
  for (const auto& pair : local_streams_) {
    const auto& local_stream = pair.second;
    blink::WebVector<blink::WebMediaStreamTrack> local_tracks;
    local_stream.AudioTracks(local_tracks);
    for (const auto& local_track : local_tracks) {
      senders.push_back(
          std::unique_ptr<blink::WebRTCRtpSender>(new MockWebRTCRtpSender(
              GetIDByTrack(local_track.Id().Utf8(), &id_by_track_),
              base::MakeUnique<WebMediaStreamTrack>(local_track))));
    }
    local_stream.VideoTracks(local_tracks);
    for (const auto& local_track : local_tracks) {
      senders.push_back(
          std::unique_ptr<blink::WebRTCRtpSender>(new MockWebRTCRtpSender(
              GetIDByTrack(local_track.Id().Utf8(), &id_by_track_),
              base::MakeUnique<WebMediaStreamTrack>(local_track))));
    }
  }
  // Senders of tracks in |tracks_| (from |Add/RemoveTrack|).
  for (const auto& pair : tracks_) {
    const auto& track = pair.second;
    bool has_sender_for_track = false;
    for (const auto& sender : senders) {
      if (sender->Track()->Id() == track.Id()) {
        has_sender_for_track = true;
        break;
      }
    }
    if (has_sender_for_track)
      continue;
    senders.push_back(base::MakeUnique<MockWebRTCRtpSender>(
        GetIDByTrack(track.Id().Utf8(), &id_by_track_),
        base::MakeUnique<WebMediaStreamTrack>(track)));
  }
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpSender>> web_vector(
      senders.size());
  for (size_t i = 0; i < senders.size(); ++i) {
    web_vector[i] = std::move(senders[i]);
  }
  return web_vector;
}

blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>>
MockWebRTCPeerConnectionHandler::GetReceivers() {
  std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>> receivers;
  for (const auto& pair : remote_streams_) {
    const auto& remote_stream = pair.second;
    for (auto& receiver : GetReceiversOfStream(remote_stream, &id_by_track_)) {
      receivers.push_back(std::move(receiver));
    }
  }
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>> web_vector(
      receivers.size());
  for (size_t i = 0; i < receivers.size(); ++i) {
    web_vector[i] = std::move(receivers[i]);
  }
  return web_vector;
}

std::unique_ptr<blink::WebRTCRtpSender>
MockWebRTCPeerConnectionHandler::AddTrack(
    const blink::WebMediaStreamTrack& web_track,
    const blink::WebVector<blink::WebMediaStream>& web_streams) {
  for (const auto& sender : GetSenders()) {
    if (sender->Track() && sender->Track()->Id() == web_track.Id()) {
      return nullptr;
    }
  }
  tracks_[web_track.Id().Utf8()] = web_track;
  client_->NegotiationNeeded();
  return base::MakeUnique<MockWebRTCRtpSender>(
      GetIDByTrack(web_track.Id().Utf8(), &id_by_track_),
      base::MakeUnique<blink::WebMediaStreamTrack>(web_track));
}

bool MockWebRTCPeerConnectionHandler::RemoveTrack(
    blink::WebRTCRtpSender* web_sender) {
  if (!tracks_.erase(web_sender->Track()->Id().Utf8()))
    return false;
  MockWebRTCRtpSender* sender = static_cast<MockWebRTCRtpSender*>(web_sender);
  sender->SetTrack(nullptr);
  client_->NegotiationNeeded();
  return true;
}

void MockWebRTCPeerConnectionHandler::ReportCreationOfDataChannel() {
  WebRTCDataChannelInit init;
  WebRTCDataChannelHandler* remote_data_channel =
      new MockWebRTCDataChannelHandler("MockRemoteDataChannel", init,
                                       interfaces_->GetDelegate());
  client_->DidAddRemoteDataChannel(remote_data_channel);
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::CreateDataChannel(
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

WebRTCDTMFSenderHandler* MockWebRTCPeerConnectionHandler::CreateDTMFSender(
    const WebMediaStreamTrack& track) {
  return new MockWebRTCDTMFSenderHandler(track, interfaces_->GetDelegate());
}

void MockWebRTCPeerConnectionHandler::Stop() {
  stopped_ = true;
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace test_runner
