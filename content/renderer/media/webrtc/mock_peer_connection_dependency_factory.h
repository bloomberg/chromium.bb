// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_PEER_CONNECTION_DEPENDENCY_FACTORY_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "third_party/webrtc/api/mediaconstraintsinterface.h"
#include "third_party/webrtc/media/base/videorenderer.h"
#include "third_party/webrtc/media/base/videosinkinterface.h"

namespace content {

class WebAudioCapturerSource;
typedef std::set<webrtc::ObserverInterface*> ObserverSet;

class MockAudioSource : public webrtc::AudioSourceInterface {
 public:
  explicit MockAudioSource(const cricket::AudioOptions& options, bool remote);

  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;
  MediaSourceInterface::SourceState state() const override;
  bool remote() const override;

  // Changes the state of the source to live and notifies the observer.
  void SetLive();
  // Changes the state of the source to ended and notifies the observer.
  void SetEnded();

 protected:
  ~MockAudioSource() override;

 private:
  bool remote_;
  ObserverSet observers_;
  MediaSourceInterface::SourceState state_;
};

class MockWebRtcVideoTrack : public webrtc::VideoTrackInterface {
 public:
  MockWebRtcVideoTrack(const std::string& id,
                       webrtc::VideoTrackSourceInterface* source);
  void AddOrUpdateSink(rtc::VideoSinkInterface<cricket::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<cricket::VideoFrame>* sink) override;
  std::string kind() const override;
  std::string id() const override;
  bool enabled() const override;
  TrackState state() const override;
  bool set_enabled(bool enable) override;
  bool set_state(TrackState new_state) override;
  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;
  webrtc::VideoTrackSourceInterface* GetSource() const override;

 protected:
  ~MockWebRtcVideoTrack() override;

 private:
  bool enabled_;
  std::string id_;
  TrackState state_;
  scoped_refptr<webrtc::VideoTrackSourceInterface> source_;
  ObserverSet observers_;
  rtc::VideoSinkInterface<cricket::VideoFrame>* sink_;
};

class MockMediaStream : public webrtc::MediaStreamInterface {
 public:
  explicit MockMediaStream(const std::string& label);

  bool AddTrack(webrtc::AudioTrackInterface* track) override;
  bool AddTrack(webrtc::VideoTrackInterface* track) override;
  bool RemoveTrack(webrtc::AudioTrackInterface* track) override;
  bool RemoveTrack(webrtc::VideoTrackInterface* track) override;
  std::string label() const override;
  webrtc::AudioTrackVector GetAudioTracks() override;
  webrtc::VideoTrackVector GetVideoTracks() override;
  rtc::scoped_refptr<webrtc::AudioTrackInterface> FindAudioTrack(
      const std::string& track_id) override;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> FindVideoTrack(
      const std::string& track_id) override;
  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;

 protected:
  ~MockMediaStream() override;

 private:
  void NotifyObservers();

  std::string label_;
  webrtc::AudioTrackVector audio_track_vector_;
  webrtc::VideoTrackVector video_track_vector_;

  ObserverSet observers_;
};

// A mock factory for creating different objects for
// RTC PeerConnections.
class MockPeerConnectionDependencyFactory
     : public PeerConnectionDependencyFactory {
 public:
  MockPeerConnectionDependencyFactory();
  ~MockPeerConnectionDependencyFactory() override;

  scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      blink::WebFrame* frame,
      webrtc::PeerConnectionObserver* observer) override;
  scoped_refptr<webrtc::AudioSourceInterface> CreateLocalAudioSource(
      const cricket::AudioOptions& options) override;
  WebRtcVideoCapturerAdapter* CreateVideoCapturer(
      bool is_screen_capture) override;
  scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer) override;
  void CreateWebAudioSource(blink::WebMediaStreamSource* source) override;
  scoped_refptr<webrtc::MediaStreamInterface> CreateLocalMediaStream(
      const std::string& label) override;
  scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack(
      const std::string& id,
      webrtc::VideoTrackSourceInterface* source) override;
  scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack(
      const std::string& id,
      cricket::VideoCapturer* capturer) override;
  webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& type,
      const std::string& sdp,
      webrtc::SdpParseError* error) override;
  webrtc::IceCandidateInterface* CreateIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp) override;

  scoped_ptr<WebRtcAudioCapturer> CreateAudioCapturer(
      int render_frame_id,
      const StreamDeviceInfo& device_info,
      const blink::WebMediaConstraints& constraints,
      MediaStreamAudioSource* audio_source) override;
  void FailToCreateNextAudioCapturer() {
    fail_to_create_next_audio_capturer_ = true;
  }

  MockAudioSource* last_audio_source() { return last_audio_source_.get(); }

 private:
  bool fail_to_create_next_audio_capturer_;
  scoped_refptr <MockAudioSource> last_audio_source_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionDependencyFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
