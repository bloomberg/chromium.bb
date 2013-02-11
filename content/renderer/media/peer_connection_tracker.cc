// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/peer_connection_tracker.h"

#include "base/utf_string_conversions.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCICECandidate.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

using std::string;
using webrtc::MediaConstraintsInterface;
using WebKit::WebRTCPeerConnectionHandlerClient;

namespace content {

static string SerializeServers(
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers) {
  string result = "[";
  for (size_t i = 0; i < servers.size(); ++i) {
    result += servers[i].uri;
    if (i != servers.size() - 1)
      result += ", ";
  }
  result += "]";
  return result;
}

static string SerializeMediaConstraints(
    const RTCMediaConstraints& constraints) {
  string result;
  MediaConstraintsInterface::Constraints mandatory = constraints.GetMandatory();
  if (!mandatory.empty()) {
    result += "mandatory: {";
    for (size_t i = 0; i < mandatory.size(); ++i) {
      result += mandatory[i].key + ":" + mandatory[i].value;
      if (i != mandatory.size() - 1)
        result += ", ";
    }
    result += "}";
  }
  MediaConstraintsInterface::Constraints optional = constraints.GetOptional();
  if (!optional.empty()) {
    if (!result.empty())
      result += ", ";
    result += "optional: {";
    for (size_t i = 0; i < optional.size(); ++i) {
      result += optional[i].key + ":" + optional[i].value;
      if (i != optional.size() - 1)
        result += ", ";
    }
    result += "}";
  }
  return result;
}

static string SerializeMediaStreamComponent(
    const WebKit::WebMediaStreamTrack component) {
  string id = UTF16ToUTF8(component.source().id());
  return id;
}

static string SerializeMediaDescriptor(
    const WebKit::WebMediaStream& stream) {
  string label = UTF16ToUTF8(stream.label());
  string result = "label: " + label;
  WebKit::WebVector<WebKit::WebMediaStreamTrack> sources;
  stream.audioSources(sources);
  if (!sources.isEmpty()) {
    result += ", audio: [";
    for (size_t i = 0; i < sources.size(); ++i) {
      result += SerializeMediaStreamComponent(sources[i]);
      if (i != sources.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  stream.videoSources(sources);
  if (!sources.isEmpty()) {
    result += ", video: [";
    for (size_t i = 0; i < sources.size(); ++i) {
      result += SerializeMediaStreamComponent(sources[i]);
      if (i != sources.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  return result;
}

#define GET_STRING_OF_STATE(state)                \
  case WebRTCPeerConnectionHandlerClient::state:  \
    result = #state;                              \
    break;

static string GetSignalingStateString(
    WebRTCPeerConnectionHandlerClient::SignalingState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(SignalingStateStable)
    GET_STRING_OF_STATE(SignalingStateHaveLocalOffer)
    GET_STRING_OF_STATE(SignalingStateHaveRemoteOffer)
    GET_STRING_OF_STATE(SignalingStateHaveLocalPrAnswer)
    GET_STRING_OF_STATE(SignalingStateHaveRemotePrAnswer)
    GET_STRING_OF_STATE(SignalingStateClosed)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

static string GetIceStateString(
    WebRTCPeerConnectionHandlerClient::ICEState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(ICEStateStarting)
    GET_STRING_OF_STATE(ICEStateChecking)
    GET_STRING_OF_STATE(ICEStateConnected)
    GET_STRING_OF_STATE(ICEStateCompleted)
    GET_STRING_OF_STATE(ICEStateFailed)
    GET_STRING_OF_STATE(ICEStateDisconnected)
    GET_STRING_OF_STATE(ICEStateClosed)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

// Builds a DictionaryValue from the StatsElement.
// The caller takes the ownership of the returned value.
static DictionaryValue* GetDictValue(const webrtc::StatsElement& elem) {
  if (elem.values.empty())
    return NULL;

  DictionaryValue* dict = new DictionaryValue();
  if (!dict)
    return NULL;
  dict->SetDouble("timestamp", elem.timestamp);

  ListValue* values = new ListValue();
  if (!values) {
    delete dict;
    return NULL;
  }
  dict->Set("values", values);

  for (size_t i = 0; i < elem.values.size(); ++i) {
    values->AppendString(elem.values[i].name);
    values->AppendString(elem.values[i].value);
  }
  return dict;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
static DictionaryValue* GetDictValue(const webrtc::StatsReport& report) {
  scoped_ptr<DictionaryValue> local, remote, result;

  local.reset(GetDictValue(report.local));
  remote.reset(GetDictValue(report.remote));
  if (!local.get() && !remote.get())
    return NULL;

  result.reset(new DictionaryValue());
  if (!result.get())
    return NULL;

  if (local.get())
    result->Set("local", local.release());
  if (remote.get())
    result->Set("remote", remote.release());
  result->SetString("id", report.id);
  result->SetString("type", report.type);

  return result.release();
}

class InternalStatsObserver : public webrtc::StatsObserver {
 public:
  InternalStatsObserver(int lid)
      : lid_(lid){}

  virtual void OnComplete(
      const std::vector<webrtc::StatsReport>& reports) OVERRIDE {
    ListValue list;

    for (size_t i = 0; i < reports.size(); ++i) {
      DictionaryValue* report = GetDictValue(reports[i]);
      if (report)
        list.Append(report);
    }

    if (!list.empty())
      RenderThreadImpl::current()->Send(
          new PeerConnectionTrackerHost_AddStats(lid_, list));
  }

 protected:
  virtual ~InternalStatsObserver() {}

 private:
  int lid_;
};

PeerConnectionTracker::PeerConnectionTracker() : next_lid_(1) {
}

PeerConnectionTracker::~PeerConnectionTracker() {
}

bool PeerConnectionTracker::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PeerConnectionTracker, message)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_GetAllStats, OnGetAllStats)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PeerConnectionTracker::OnGetAllStats() {
  for (PeerConnectionIdMap::iterator it = peer_connection_id_map_.begin();
       it != peer_connection_id_map_.end(); ++it) {

    talk_base::scoped_refptr<InternalStatsObserver> observer(
        new talk_base::RefCountedObject<InternalStatsObserver>(it->second));

    it->first->GetStats(observer, NULL);
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers,
    const RTCMediaConstraints& constraints,
    const WebKit::WebFrame* frame) {
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  PeerConnectionInfo info;

  info.lid = GetNextLocalID();
  info.servers = SerializeServers(servers);
  info.constraints = SerializeMediaConstraints(constraints);
  info.url = frame->document().url().spec();
  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_AddPeerConnection(info));

  DCHECK(peer_connection_id_map_.find(pc_handler) ==
         peer_connection_id_map_.end());
  peer_connection_id_map_[pc_handler] = info.lid;
}

void PeerConnectionTracker::UnregisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler) {
  DVLOG(1) << "PeerConnectionTracker::UnregisterPeerConnection()";

  std::map<RTCPeerConnectionHandler*, int>::iterator it =
      peer_connection_id_map_.find(pc_handler);

  if (it == peer_connection_id_map_.end()) {
    // The PeerConnection might not have been registered if its initilization
    // failed.
    return;
  }

  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_RemovePeerConnection(it->second));

  peer_connection_id_map_.erase(it);
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const RTCMediaConstraints& constraints) {
  SendPeerConnectionUpdate(
      pc_handler, "createOffer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const RTCMediaConstraints& constraints) {
  SendPeerConnectionUpdate(
      pc_handler, "createAnswer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackSetSessionDescription(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::SessionDescriptionInterface* desc,
    Source source) {
  string sdp;
  desc->ToString(&sdp);

  string value = "type: " + desc->type() + ", sdp: " + sdp;
  SendPeerConnectionUpdate(
      pc_handler,
      source == SOURCE_LOCAL ? "setLocalDescription" : "setRemoteDescription",
      value);
}

void PeerConnectionTracker::TrackUpdateIce(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers,
      const RTCMediaConstraints& options) {
  string servers_string = "servers: " + SerializeServers(servers);
  string constraints =
      "constraints: {" + SerializeMediaConstraints(options) + "}";

  SendPeerConnectionUpdate(
      pc_handler, "updateIce", servers_string + ", " + constraints);
}

void PeerConnectionTracker::TrackAddIceCandidate(
      RTCPeerConnectionHandler* pc_handler,
      const WebKit::WebRTCICECandidate& candidate,
      Source source) {
  string value = "mid: " + UTF16ToUTF8(candidate.sdpMid()) + ", " +
                 "candidate: " + UTF16ToUTF8(candidate.candidate());
  SendPeerConnectionUpdate(
      pc_handler,
      source == SOURCE_LOCAL ? "onIceCandidate" : "addIceCandidate", value);
}

void PeerConnectionTracker::TrackAddStream(
    RTCPeerConnectionHandler* pc_handler,
    const WebKit::WebMediaStream& stream,
    Source source){
  SendPeerConnectionUpdate(
      pc_handler, source == SOURCE_LOCAL ? "addStream" : "onAddStream",
      SerializeMediaDescriptor(stream));
}

void PeerConnectionTracker::TrackRemoveStream(
    RTCPeerConnectionHandler* pc_handler,
    const WebKit::WebMediaStream& stream,
    Source source){
  SendPeerConnectionUpdate(
      pc_handler, source == SOURCE_LOCAL ? "removeStream" : "onRemoveStream",
      SerializeMediaDescriptor(stream));
}

void PeerConnectionTracker::TrackCreateDataChannel(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::DataChannelInterface* data_channel,
    Source source) {
  string value = "label: " + data_channel->label() +
                 ", reliable: " + (data_channel->reliable() ? "true" : "false");
  SendPeerConnectionUpdate(
      pc_handler,
      source == SOURCE_LOCAL ? "createLocalDataChannel" : "onRemoteDataChannel",
      value);
}

void PeerConnectionTracker::TrackOnIceComplete(
    RTCPeerConnectionHandler* pc_handler) {
  SendPeerConnectionUpdate(pc_handler, "onIceCandidate(null)", "");
}

void PeerConnectionTracker::TrackStop(RTCPeerConnectionHandler* pc_handler) {
  SendPeerConnectionUpdate(pc_handler, "stop", "");
}

void PeerConnectionTracker::TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::SignalingState state) {
  SendPeerConnectionUpdate(
      pc_handler, "signalingStateChange", GetSignalingStateString(state));
}

void PeerConnectionTracker::TrackIceStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::ICEState state) {
  // TODO (jiayl): Should update this when the new IceState handling is done.
  SendPeerConnectionUpdate(
      pc_handler, "IceStateChange", GetIceStateString(state));
}

void PeerConnectionTracker::TrackSessionDescriptionCallback(
    RTCPeerConnectionHandler* pc_handler, Action action,
    const string& callback_type, const string& value) {
  string update_type;
  switch (action) {
    case ACTION_SET_LOCAL_DESCRIPTION:
      update_type = "setLocalDescription";
      break;
    case ACTION_SET_REMOTE_DESCRIPTION:
      update_type = "setRemoteDescription";
      break;
    case ACTION_CREATE_OFFER:
      update_type = "createOffer";
      break;
    case ACTION_CREATE_ANSWER:
      update_type = "createAnswer";
      break;
    default:
      NOTREACHED();
      break;
  }
  update_type += callback_type;

  SendPeerConnectionUpdate(pc_handler, update_type, value);
}

int PeerConnectionTracker::GetNextLocalID() {
  return next_lid_++;
}

void PeerConnectionTracker::SendPeerConnectionUpdate(
    RTCPeerConnectionHandler* pc_handler,
    const std::string& type,
    const std::string& value) {
  if (peer_connection_id_map_.find(pc_handler) == peer_connection_id_map_.end())
    return;

  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_UpdatePeerConnection(
          peer_connection_id_map_[pc_handler], type, value));
}

}  // namespace content
