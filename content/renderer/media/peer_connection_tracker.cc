// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/peer_connection_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

using std::string;
using webrtc::MediaConstraintsInterface;
using webrtc::StatsReport;
using webrtc::StatsReports;
using blink::WebRTCPeerConnectionHandlerClient;

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

static RTCMediaConstraints GetNativeMediaConstraints(
    const blink::WebMediaConstraints& constraints) {
  RTCMediaConstraints native_constraints;

  if (constraints.isNull())
    return native_constraints;

  blink::WebVector<blink::WebMediaConstraint> mandatory;
  constraints.getMandatoryConstraints(mandatory);
  for (size_t i = 0; i < mandatory.size(); ++i) {
    native_constraints.AddMandatory(
        mandatory[i].m_name.utf8(), mandatory[i].m_value.utf8(), false);
  }

  blink::WebVector<blink::WebMediaConstraint> optional;
  constraints.getOptionalConstraints(optional);
  for (size_t i = 0; i < optional.size(); ++i) {
    native_constraints.AddOptional(
        optional[i].m_name.utf8(), optional[i].m_value.utf8(), false);
  }
  return native_constraints;
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
    const blink::WebMediaStreamTrack component) {
  string id = base::UTF16ToUTF8(base::StringPiece16(component.source().id()));
  return id;
}

static string SerializeMediaDescriptor(
    const blink::WebMediaStream& stream) {
  string label = base::UTF16ToUTF8(base::StringPiece16(stream.id()));
  string result = "label: " + label;
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  stream.audioTracks(tracks);
  if (!tracks.isEmpty()) {
    result += ", audio: [";
    for (size_t i = 0; i < tracks.size(); ++i) {
      result += SerializeMediaStreamComponent(tracks[i]);
      if (i != tracks.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  stream.videoTracks(tracks);
  if (!tracks.isEmpty()) {
    result += ", video: [";
    for (size_t i = 0; i < tracks.size(); ++i) {
      result += SerializeMediaStreamComponent(tracks[i]);
      if (i != tracks.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  return result;
}

static std::string SerializeIceTransportType(
    webrtc::PeerConnectionInterface::IceTransportsType type) {
  string transport_type;
  switch (type) {
  case webrtc::PeerConnectionInterface::kNone:
    transport_type = "none";
    break;
  case webrtc::PeerConnectionInterface::kRelay:
    transport_type = "relay";
    break;
  case webrtc::PeerConnectionInterface::kAll:
    transport_type = "all";
    break;
  case webrtc::PeerConnectionInterface::kNoHost:
    transport_type = "noHost";
    break;
  default:
    NOTREACHED();
  };
  return transport_type;
}

static std::string SerializeBundlePolicy(
    webrtc::PeerConnectionInterface::BundlePolicy policy) {
  string policy_str;
  switch (policy) {
  case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
    policy_str = "balanced";
    break;
  case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
    policy_str = "max-bundle";
    break;
  case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
    policy_str = "max-compat";
    break;
  default:
    NOTREACHED();
  };
  return policy_str;
}

static std::string SerializeRtcpMuxPolicy(
    webrtc::PeerConnectionInterface::RtcpMuxPolicy policy) {
  string policy_str;
  switch (policy) {
  case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
    policy_str = "negotiate";
    break;
  case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
    policy_str = "require";
    break;
  default:
    NOTREACHED();
  };
  return policy_str;
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

static string GetIceConnectionStateString(
    WebRTCPeerConnectionHandlerClient::ICEConnectionState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(ICEConnectionStateStarting)
    GET_STRING_OF_STATE(ICEConnectionStateChecking)
    GET_STRING_OF_STATE(ICEConnectionStateConnected)
    GET_STRING_OF_STATE(ICEConnectionStateCompleted)
    GET_STRING_OF_STATE(ICEConnectionStateFailed)
    GET_STRING_OF_STATE(ICEConnectionStateDisconnected)
    GET_STRING_OF_STATE(ICEConnectionStateClosed)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

static string GetIceGatheringStateString(
    WebRTCPeerConnectionHandlerClient::ICEGatheringState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(ICEGatheringStateNew)
    GET_STRING_OF_STATE(ICEGatheringStateGathering)
    GET_STRING_OF_STATE(ICEGatheringStateComplete)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
// Note:
// The format must be consistent with what webrtc_internals.js expects.
// If you change it here, you must change webrtc_internals.js as well.
static base::DictionaryValue* GetDictValueStats(const StatsReport& report) {
  if (report.values().empty())
    return NULL;

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetDouble("timestamp", report.timestamp());

  base::ListValue* values = new base::ListValue();
  dict->Set("values", values);

  for (const auto& v : report.values()) {
    const StatsReport::ValuePtr& value = v.second;
    values->AppendString(value->display_name());
    switch (value->type()) {
      case StatsReport::Value::kInt:
        values->AppendInteger(value->int_val());
        break;
      case StatsReport::Value::kFloat:
        values->AppendDouble(value->float_val());
        break;
      case StatsReport::Value::kString:
        values->AppendString(value->string_val());
        break;
      case StatsReport::Value::kStaticString:
        values->AppendString(value->static_string_val());
        break;
      case StatsReport::Value::kBool:
        values->AppendBoolean(value->bool_val());
        break;
      case StatsReport::Value::kInt64:  // int64_t isn't supported, so use
                                        // string.
      case StatsReport::Value::kId:
      default:
        values->AppendString(value->ToString());
        break;
    }
  }

  return dict;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
static base::DictionaryValue* GetDictValue(const StatsReport& report) {
  scoped_ptr<base::DictionaryValue> stats, result;

  stats.reset(GetDictValueStats(report));
  if (!stats)
    return NULL;

  result.reset(new base::DictionaryValue());
  // Note:
  // The format must be consistent with what webrtc_internals.js expects.
  // If you change it here, you must change webrtc_internals.js as well.
  result->Set("stats", stats.release());
  result->SetString("id", report.id()->ToString());
  result->SetString("type", report.TypeToString());

  return result.release();
}

class InternalStatsObserver : public webrtc::StatsObserver {
 public:
  InternalStatsObserver(int lid)
      : lid_(lid), main_thread_(base::ThreadTaskRunnerHandle::Get()) {}

  void OnComplete(const StatsReports& reports) override {
    scoped_ptr<base::ListValue> list(new base::ListValue());

    for (const auto* r : reports) {
      base::DictionaryValue* report = GetDictValue(*r);
      if (report)
        list->Append(report);
    }

    if (!list->empty()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&InternalStatsObserver::OnCompleteImpl,
                     base::Passed(&list), lid_));
    }
  }

 protected:
  ~InternalStatsObserver() override {
    // Will be destructed on libjingle's signaling thread.
    // The signaling thread is where libjingle's objects live and from where
    // libjingle makes callbacks.  This may or may not be the same thread as
    // the main thread.
  }

 private:
  // Static since |this| will most likely have been deleted by the time we
  // get here.
  static void OnCompleteImpl(scoped_ptr<base::ListValue> list, int lid) {
    DCHECK(!list->empty());
    RenderThreadImpl::current()->Send(
        new PeerConnectionTrackerHost_AddStats(lid, *list.get()));
  }

  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
};

PeerConnectionTracker::PeerConnectionTracker() : next_local_id_(1) {
}

PeerConnectionTracker::~PeerConnectionTracker() {
}

bool PeerConnectionTracker::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PeerConnectionTracker, message)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_GetAllStats, OnGetAllStats)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_OnSuspend, OnSuspend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PeerConnectionTracker::OnGetAllStats() {
  DCHECK(main_thread_.CalledOnValidThread());

  const std::string empty_track_id;
  for (PeerConnectionIdMap::iterator it = peer_connection_id_map_.begin();
       it != peer_connection_id_map_.end(); ++it) {
    rtc::scoped_refptr<InternalStatsObserver> observer(
        new rtc::RefCountedObject<InternalStatsObserver>(it->second));

    // The last type parameter is ignored when the track id is empty.
    it->first->GetStats(
        observer,
        webrtc::PeerConnectionInterface::kStatsOutputLevelDebug,
        empty_track_id, blink::WebMediaStreamSource::TypeAudio);
  }
}

void PeerConnectionTracker::OnSuspend() {
  DCHECK(main_thread_.CalledOnValidThread());
  for (PeerConnectionIdMap::iterator it = peer_connection_id_map_.begin();
       it != peer_connection_id_map_.end(); ++it) {
    it->first->CloseClientPeerConnection();
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const RTCMediaConstraints& constraints,
    const blink::WebFrame* frame) {
  DCHECK(main_thread_.CalledOnValidThread());
  DCHECK_EQ(GetLocalIDForHandler(pc_handler), -1);
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  PeerConnectionInfo info;

  info.lid = GetNextLocalID();
  info.rtc_configuration =
      "{ servers: " +  SerializeServers(config.servers) + ", " +
      "iceTransportType: " + SerializeIceTransportType(config.type) + ", " +
      "bundlePolicy: " + SerializeBundlePolicy(config.bundle_policy) + ", " +
      "rtcpMuxPolicy: " + SerializeRtcpMuxPolicy(config.rtcp_mux_policy) + " }";

  info.constraints = SerializeMediaConstraints(constraints);
  info.url = frame->document().url().string().utf8();
  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_AddPeerConnection(info));

  peer_connection_id_map_.insert(std::make_pair(pc_handler, info.lid));
}

void PeerConnectionTracker::UnregisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK(main_thread_.CalledOnValidThread());
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
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createOffer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const RTCMediaConstraints& constraints) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createAnswer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackSetSessionDescription(
    RTCPeerConnectionHandler* pc_handler,
    const std::string& sdp, const std::string& type, Source source) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  string value = "type: " + type + ", sdp: " + sdp;
  SendPeerConnectionUpdate(
      id,
      source == SOURCE_LOCAL ? "setLocalDescription" : "setRemoteDescription",
      value);
}

void PeerConnectionTracker::TrackUpdateIce(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      const RTCMediaConstraints& options) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  string servers_string = "servers: " + SerializeServers(config.servers);

  string transport_type =
      "iceTransportType: " + SerializeIceTransportType(config.type);

  string bundle_policy =
      "bundlePolicy: " + SerializeBundlePolicy(config.bundle_policy);

  string rtcp_mux_policy =
      "rtcpMuxPolicy: " + SerializeRtcpMuxPolicy(config.rtcp_mux_policy);

  string constraints =
      "constraints: {" + SerializeMediaConstraints(options) + "}";

  SendPeerConnectionUpdate(
      id,
      "updateIce",
      servers_string + ", " + transport_type + ", " +
      bundle_policy + ", " + rtcp_mux_policy + ", " +
      constraints);
}

void PeerConnectionTracker::TrackAddIceCandidate(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebRTCICECandidate& candidate,
      Source source,
      bool succeeded) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  string value =
      "sdpMid: " +
      base::UTF16ToUTF8(base::StringPiece16(candidate.sdpMid())) + ", " +
      "sdpMLineIndex: " + base::UintToString(candidate.sdpMLineIndex()) +
      ", " + "candidate: " +
      base::UTF16ToUTF8(base::StringPiece16(candidate.candidate()));

  // OnIceCandidate always succeeds as it's a callback from the browser.
  DCHECK(source != SOURCE_LOCAL || succeeded);

  const char* event =
      (source == SOURCE_LOCAL) ? "onIceCandidate"
                               : (succeeded ? "addIceCandidate"
                                            : "addIceCandidateFailed");

  SendPeerConnectionUpdate(id, event, value);
}

void PeerConnectionTracker::TrackAddStream(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaStream& stream,
    Source source) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, source == SOURCE_LOCAL ? "addStream" : "onAddStream",
      SerializeMediaDescriptor(stream));
}

void PeerConnectionTracker::TrackRemoveStream(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaStream& stream,
    Source source){
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, source == SOURCE_LOCAL ? "removeStream" : "onRemoveStream",
      SerializeMediaDescriptor(stream));
}

void PeerConnectionTracker::TrackCreateDataChannel(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::DataChannelInterface* data_channel,
    Source source) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  string value = "label: " + data_channel->label() +
                 ", reliable: " + (data_channel->reliable() ? "true" : "false");
  SendPeerConnectionUpdate(
      id,
      source == SOURCE_LOCAL ? "createLocalDataChannel" : "onRemoteDataChannel",
      value);
}

void PeerConnectionTracker::TrackStop(RTCPeerConnectionHandler* pc_handler) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "stop", std::string());
}

void PeerConnectionTracker::TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::SignalingState state) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "signalingStateChange", GetSignalingStateString(state));
}

void PeerConnectionTracker::TrackIceConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::ICEConnectionState state) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "iceConnectionStateChange",
      GetIceConnectionStateString(state));
}

void PeerConnectionTracker::TrackIceGatheringStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::ICEGatheringState state) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "iceGatheringStateChange",
      GetIceGatheringStateString(state));
}

void PeerConnectionTracker::TrackSessionDescriptionCallback(
    RTCPeerConnectionHandler* pc_handler, Action action,
    const string& callback_type, const string& value) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
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

  SendPeerConnectionUpdate(id, update_type.c_str(), value);
}

void PeerConnectionTracker::TrackOnRenegotiationNeeded(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "onRenegotiationNeeded", std::string());
}

void PeerConnectionTracker::TrackCreateDTMFSender(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaStreamTrack& track) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "createDTMFSender",
                           base::UTF16ToUTF8(base::StringPiece16(track.id())));
}

void PeerConnectionTracker::TrackGetUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(main_thread_.CalledOnValidThread());
  RTCMediaConstraints audio_constraints(
      GetNativeMediaConstraints(user_media_request.audioConstraints()));
  RTCMediaConstraints video_constraints(
      GetNativeMediaConstraints(user_media_request.videoConstraints()));

  RenderThreadImpl::current()->Send(new PeerConnectionTrackerHost_GetUserMedia(
      user_media_request.securityOrigin().toString().utf8(),
      user_media_request.audio(),
      user_media_request.video(),
      SerializeMediaConstraints(audio_constraints),
      SerializeMediaConstraints(video_constraints)));
}

int PeerConnectionTracker::GetNextLocalID() {
  DCHECK(main_thread_.CalledOnValidThread());
  if (next_local_id_< 0)
    next_local_id_ = 1;
  return next_local_id_++;
}

int PeerConnectionTracker::GetLocalIDForHandler(
    RTCPeerConnectionHandler* handler) const {
  DCHECK(main_thread_.CalledOnValidThread());
  const auto found = peer_connection_id_map_.find(handler);
  if (found == peer_connection_id_map_.end())
    return -1;
  DCHECK_NE(found->second, -1);
  return found->second;
}

void PeerConnectionTracker::SendPeerConnectionUpdate(
    int local_id,
    const char* callback_type,
    const std::string& value) {
  DCHECK(main_thread_.CalledOnValidThread());
  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_UpdatePeerConnection(
          local_id, std::string(callback_type), value));
}

}  // namespace content
