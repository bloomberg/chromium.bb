// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_
#define CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_

#include <map>

#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"

namespace WebKit {
class WebFrame;
class WebRTCICECandidate;
class WebString;
}  // namespace WebKit

namespace webrtc {
class DataChannelInterface;
}  // namespace webrtc

namespace content {
class RTCMediaConstraints;
class RTCPeerConnectionHandler;

// This class collects data about each peer connection,
// sends it to the browser process, and handles messages
// from the browser process.
class PeerConnectionTracker : public RenderProcessObserver{
 public:
  PeerConnectionTracker();
  virtual ~PeerConnectionTracker();

  enum Source {
    SOURCE_LOCAL,
    SOURCE_REMOTE
  };

  enum Action {
    ACTION_SET_LOCAL_DESCRIPTION,
    ACTION_SET_REMOTE_DESCRIPTION,
    ACTION_CREATE_OFFER,
    ACTION_CREATE_ANSWER
  };

  //
  // The following methods send an update to the browser process when a
  // PeerConnection update happens. The caller should call the Track* methods
  // after calling RegisterPeerConnection and before calling
  // UnregisterPeerConnection, otherwise the Track* call has no effect.
  //

  // Sends an update when a PeerConnection has been created in Javascript.
  // This should be called once and only once for each PeerConnection.
  // The |pc_handler| is the handler object associated with the PeerConnection,
  // the |servers| are the server configurations used to establish the
  // connection, the |constraints| are the media constraints used to initialize
  // the PeerConnection, the |frame| is the WebFrame object representing the
  // page in which the PeerConnection is created.
  void RegisterPeerConnection(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::JsepInterface::IceServer>& servers,
      const RTCMediaConstraints& constraints,
      const WebKit::WebFrame* frame);

  // Sends an update when a PeerConnection has been destroyed.
  void UnregisterPeerConnection(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when createOffer/createAnswer has been called.
  // The |pc_handler| is the handler object associated with the PeerConnection,
  // the |constraints| is the media constraints used to create the offer/answer.
  void TrackCreateOffer(RTCPeerConnectionHandler* pc_handler,
                        const RTCMediaConstraints& constraints);
  void TrackCreateAnswer(RTCPeerConnectionHandler* pc_handler,
                         const RTCMediaConstraints& constraints);

  // Sends an update when setLocalDescription or setRemoteDescription is called.
  void TrackSetSessionDescription(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::SessionDescriptionInterface* desc, Source source);

  // Sends an update when Ice candidates are updated.
  void TrackUpdateIce(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::JsepInterface::IceServer>& servers,
      const RTCMediaConstraints& options);

  // Sends an update when an Ice candidate is added.
  void TrackAddIceCandidate(
      RTCPeerConnectionHandler* pc_handler,
      const WebKit::WebRTCICECandidate& candidate, Source source);

  // Sends an update when a media stream is added.
  void TrackAddStream(
      RTCPeerConnectionHandler* pc_handler,
      const WebKit::WebMediaStream& stream, Source source);

  // Sends an update when a media stream is removed.
  void TrackRemoveStream(
      RTCPeerConnectionHandler* pc_handler,
      const WebKit::WebMediaStream& stream, Source source);

  // Sends an update when OnIceComplete is called.
  void TrackOnIceComplete(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when a DataChannel is created.
  void TrackCreateDataChannel(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::DataChannelInterface* data_channel, Source source);

  // Sends an update when a PeerConnection has been stopped.
  void TrackStop(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when the signaling state of a PeerConnection has changed.
  void TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebKit::WebRTCPeerConnectionHandlerClient::SignalingState state);

  // Sends an update when the Ice state of a PeerConnection has changed.
  void TrackIceStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebKit::WebRTCPeerConnectionHandlerClient::ICEState state);

  // Sends an update when the SetSessionDescription or CreateOffer or
  // CreateAnswer callbacks are called.
  void TrackSessionDescriptionCallback(
      RTCPeerConnectionHandler* pc_handler, Action action,
      const std::string& type, const std::string& value);

 private:
  // Assign a local ID to a peer connection so that the browser process can
  // uniquely identify a peer connection in the renderer process.
  int GetNextLocalID();

  void SendPeerConnectionUpdate(RTCPeerConnectionHandler* pc_handler,
                                const std::string& callback_type,
                                const std::string& value);

  // This map stores the local ID assigned to each RTCPeerConnectionHandler.
  std::map<RTCPeerConnectionHandler*, int> peer_connection_id_map_;

  // This keeps track of the next available local ID.
  int next_lid_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_
