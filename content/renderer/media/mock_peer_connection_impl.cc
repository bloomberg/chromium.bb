// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_peer_connection_impl.h"

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "content/renderer/media/mock_data_channel_impl.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "third_party/webrtc/api/rtpreceiverinterface.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"

using testing::_;
using webrtc::AudioTrackInterface;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;

namespace content {

class MockStreamCollection : public webrtc::StreamCollectionInterface {
 public:
  size_t count() override { return streams_.size(); }
  MediaStreamInterface* at(size_t index) override { return streams_[index]; }
  MediaStreamInterface* find(const std::string& label) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      if (streams_[i]->label() == label)
        return streams_[i];
    }
    return NULL;
  }
  webrtc::MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindAudioTrack(id);
      if (track)
        return track;
    }
    return NULL;
  }
  webrtc::MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindVideoTrack(id);
      if (track)
        return track;
    }
    return NULL;
  }
  void AddStream(MediaStreamInterface* stream) {
    streams_.push_back(stream);
  }
  void RemoveStream(MediaStreamInterface* stream) {
    StreamVector::iterator it = streams_.begin();
    for (; it != streams_.end(); ++it) {
      if (it->get() == stream) {
        streams_.erase(it);
        break;
      }
    }
  }

 protected:
  ~MockStreamCollection() override {}

 private:
  typedef std::vector<rtc::scoped_refptr<MediaStreamInterface> >
      StreamVector;
  StreamVector streams_;
};

class MockDtmfSender : public DtmfSenderInterface {
 public:
  explicit MockDtmfSender(AudioTrackInterface* track)
      : track_(track),
        observer_(NULL),
        duration_(0),
        inter_tone_gap_(0) {}
  void RegisterObserver(DtmfSenderObserverInterface* observer) override {
    observer_ = observer;
  }
  void UnregisterObserver() override { observer_ = NULL; }
  bool CanInsertDtmf() override { return true; }
  bool InsertDtmf(const std::string& tones,
                  int duration,
                  int inter_tone_gap) override {
    tones_ = tones;
    duration_ = duration;
    inter_tone_gap_ = inter_tone_gap;
    return true;
  }
  const AudioTrackInterface* track() const override { return track_.get(); }
  std::string tones() const override { return tones_; }
  int duration() const override { return duration_; }
  int inter_tone_gap() const override { return inter_tone_gap_; }

 protected:
  ~MockDtmfSender() override {}

 private:
  rtc::scoped_refptr<AudioTrackInterface> track_;
  DtmfSenderObserverInterface* observer_;
  std::string tones_;
  int duration_;
  int inter_tone_gap_;
};

class FakeRtpReceiver : public webrtc::RtpReceiverInterface {
 public:
  FakeRtpReceiver(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track)
      : track_(track) {}

  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const override {
    return track_;
  }

  cricket::MediaType media_type() const override {
    NOTIMPLEMENTED();
    return cricket::MEDIA_TYPE_AUDIO;
  }

  std::string id() const override {
    NOTIMPLEMENTED();
    return "";
  }

  webrtc::RtpParameters GetParameters() const override {
    NOTIMPLEMENTED();
    return webrtc::RtpParameters();
  }

  bool SetParameters(const webrtc::RtpParameters& parameters) override {
    NOTIMPLEMENTED();
    return false;
  }

  void SetObserver(webrtc::RtpReceiverObserverInterface* observer) override {
    NOTIMPLEMENTED();
  }

 private:
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
};

const char MockPeerConnectionImpl::kDummyOffer[] = "dummy offer";
const char MockPeerConnectionImpl::kDummyAnswer[] = "dummy answer";

MockPeerConnectionImpl::MockPeerConnectionImpl(
    MockPeerConnectionDependencyFactory* factory,
    webrtc::PeerConnectionObserver* observer)
    : dependency_factory_(factory),
      local_streams_(new rtc::RefCountedObject<MockStreamCollection>),
      remote_streams_(new rtc::RefCountedObject<MockStreamCollection>),
      hint_audio_(false),
      hint_video_(false),
      getstats_result_(true),
      sdp_mline_index_(-1),
      observer_(observer) {
  ON_CALL(*this, SetLocalDescription(_, _)).WillByDefault(testing::Invoke(
      this, &MockPeerConnectionImpl::SetLocalDescriptionWorker));
  ON_CALL(*this, SetRemoteDescription(_, _)).WillByDefault(testing::Invoke(
      this, &MockPeerConnectionImpl::SetRemoteDescriptionWorker));
}

MockPeerConnectionImpl::~MockPeerConnectionImpl() {}

rtc::scoped_refptr<webrtc::StreamCollectionInterface>
MockPeerConnectionImpl::local_streams() {
  return local_streams_;
}

rtc::scoped_refptr<webrtc::StreamCollectionInterface>
MockPeerConnectionImpl::remote_streams() {
  return remote_streams_;
}

bool MockPeerConnectionImpl::AddStream(MediaStreamInterface* local_stream) {
  DCHECK(stream_label_.empty());
  stream_label_ = local_stream->label();
  local_streams_->AddStream(local_stream);
  return true;
}

void MockPeerConnectionImpl::RemoveStream(
    MediaStreamInterface* local_stream) {
  DCHECK_EQ(stream_label_, local_stream->label());
  stream_label_.clear();
  local_streams_->RemoveStream(local_stream);
}

rtc::scoped_refptr<DtmfSenderInterface>
MockPeerConnectionImpl::CreateDtmfSender(AudioTrackInterface* track) {
  if (!track) {
    return NULL;
  }
  return new rtc::RefCountedObject<MockDtmfSender>(track);
}

std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>
MockPeerConnectionImpl::GetReceivers() const {
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> receivers;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    for (const auto& audio_track : remote_streams_->at(i)->GetAudioTracks()) {
      receivers.push_back(
          new rtc::RefCountedObject<FakeRtpReceiver>(audio_track));
    }
    for (const auto& video_track : remote_streams_->at(i)->GetVideoTracks()) {
      receivers.push_back(
          new rtc::RefCountedObject<FakeRtpReceiver>(video_track));
    }
  }
  return receivers;
}

rtc::scoped_refptr<webrtc::DataChannelInterface>
MockPeerConnectionImpl::CreateDataChannel(const std::string& label,
                      const webrtc::DataChannelInit* config) {
  return new rtc::RefCountedObject<MockDataChannel>(label, config);
}

bool MockPeerConnectionImpl::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::MediaStreamTrackInterface* track,
    StatsOutputLevel level) {
  if (!getstats_result_)
    return false;

  DCHECK_EQ(kStatsOutputLevelStandard, level);
  webrtc::StatsReport report1(webrtc::StatsReport::NewTypedId(
      webrtc::StatsReport::kStatsReportTypeSsrc, "1234"));
  webrtc::StatsReport report2(webrtc::StatsReport::NewTypedId(
      webrtc::StatsReport::kStatsReportTypeSession, "nontrack"));
  report1.set_timestamp(42);
  report1.AddString(webrtc::StatsReport::kStatsValueNameFingerprint,
                    "trackvalue");

  webrtc::StatsReports reports;
  reports.push_back(&report1);

  // If selector is given, we pass back one report.
  // If selector is not given, we pass back two.
  if (!track) {
    report2.set_timestamp(44);
    report2.AddString(webrtc::StatsReport::kStatsValueNameFingerprintAlgorithm,
                      "somevalue");
    reports.push_back(&report2);
  }

  // Note that the callback is synchronous, not asynchronous; it will
  // happen before the request call completes.
  observer->OnComplete(reports);

  return true;
}

void MockPeerConnectionImpl::GetStats(
    webrtc::RTCStatsCollectorCallback* callback) {
  DCHECK(callback);
  DCHECK(stats_report_);
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::SetGetStatsReport(webrtc::RTCStatsReport* report) {
  stats_report_ = report;
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::local_description() const {
  return local_desc_.get();
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::remote_description() const {
  return remote_desc_.get();
}

void MockPeerConnectionImpl::AddRemoteStream(MediaStreamInterface* stream) {
  remote_streams_->AddStream(stream);
}

void MockPeerConnectionImpl::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const RTCOfferAnswerOptions& options) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyOffer,
                                                    NULL));
}

void MockPeerConnectionImpl::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const RTCOfferAnswerOptions& options) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyAnswer,
                                                    NULL));
}

void MockPeerConnectionImpl::SetLocalDescriptionWorker(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  local_desc_.reset(desc);
}

void MockPeerConnectionImpl::SetRemoteDescriptionWorker(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  remote_desc_.reset(desc);
}

bool MockPeerConnectionImpl::SetConfiguration(
    const RTCConfiguration& configuration,
    webrtc::RTCError* error) {
  if (setconfiguration_error_type_ == webrtc::RTCErrorType::NONE) {
    return true;
  }
  error->set_type(setconfiguration_error_type_);
  return false;
}

bool MockPeerConnectionImpl::AddIceCandidate(
    const IceCandidateInterface* candidate) {
  sdp_mid_ = candidate->sdp_mid();
  sdp_mline_index_ = candidate->sdp_mline_index();
  return candidate->ToString(&ice_sdp_);
}

void MockPeerConnectionImpl::RegisterUMAObserver(
    webrtc::UMAObserver* observer) {
  NOTIMPLEMENTED();
}

webrtc::RTCError MockPeerConnectionImpl::SetBitrate(
    const BitrateParameters& bitrate) {
  NOTIMPLEMENTED();
  return webrtc::RTCError::OK();
}

}  // namespace content
