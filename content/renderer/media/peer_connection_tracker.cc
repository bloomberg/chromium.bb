// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCAnswerOptions.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCOfferOptions.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

using webrtc::MediaConstraintsInterface;
using webrtc::StatsReport;
using webrtc::StatsReports;
using blink::WebRTCPeerConnectionHandlerClient;

namespace content {

// TODO(hta): This module should be redesigned to reduce string copies.

static const char* SerializeBoolean(bool value) {
  return value ? "true" : "false";
}

static std::string SerializeServers(
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers) {
  std::string result = "[";
  for (size_t i = 0; i < servers.size(); ++i) {
    result += servers[i].uri;
    if (i != servers.size() - 1)
      result += ", ";
  }
  result += "]";
  return result;
}

static std::string SerializeMediaConstraints(
    const blink::WebMediaConstraints& constraints) {
  return constraints.ToString().Utf8();
}

static std::string SerializeOfferOptions(
    const blink::WebRTCOfferOptions& options) {
  if (options.IsNull())
    return "null";

  std::ostringstream result;
  result << "offerToReceiveVideo: " << options.OfferToReceiveVideo()
         << ", offerToReceiveAudio: " << options.OfferToReceiveAudio()
         << ", voiceActivityDetection: "
         << SerializeBoolean(options.VoiceActivityDetection())
         << ", iceRestart: " << SerializeBoolean(options.IceRestart());
  return result.str();
}

static std::string SerializeAnswerOptions(
    const blink::WebRTCAnswerOptions& options) {
  if (options.IsNull())
    return "null";

  std::ostringstream result;
  result << ", voiceActivityDetection: "
         << SerializeBoolean(options.VoiceActivityDetection());
  return result.str();
}

static std::string SerializeMediaStreamComponent(
    const blink::WebMediaStreamTrack& component) {
  return component.Source().Id().Utf8();
}

static std::string SerializeMediaDescriptor(
    const blink::WebMediaStream& stream) {
  std::string id = stream.Id().Utf8();
  std::string result = "id: " + id;
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  stream.AudioTracks(tracks);
  if (!tracks.IsEmpty()) {
    result += ", audio: [";
    for (size_t i = 0; i < tracks.size(); ++i) {
      result += SerializeMediaStreamComponent(tracks[i]);
      if (i != tracks.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  stream.VideoTracks(tracks);
  if (!tracks.IsEmpty()) {
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

static const char* SerializeIceTransportType(
    webrtc::PeerConnectionInterface::IceTransportsType type) {
  const char* transport_type = "";
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
  }
  return transport_type;
}

static const char* SerializeBundlePolicy(
    webrtc::PeerConnectionInterface::BundlePolicy policy) {
  const char* policy_str = "";
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
  }
  return policy_str;
}

static const char* SerializeRtcpMuxPolicy(
    webrtc::PeerConnectionInterface::RtcpMuxPolicy policy) {
  const char* policy_str = "";
  switch (policy) {
  case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
    policy_str = "negotiate";
    break;
  case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
    policy_str = "require";
    break;
  default:
    NOTREACHED();
  }
  return policy_str;
}

static std::string SerializeConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config) {
  std::ostringstream oss;
  // TODO(hbos): Add serialization of certificate.
  oss << "{ iceServers: " << SerializeServers(config.servers)
      << ", iceTransportPolicy: " << SerializeIceTransportType(config.type)
      << ", bundlePolicy: " << SerializeBundlePolicy(config.bundle_policy)
      << ", rtcpMuxPolicy: " << SerializeRtcpMuxPolicy(config.rtcp_mux_policy)
      << ", iceCandidatePoolSize: " << config.ice_candidate_pool_size << " }";
  return oss.str();
}

#define GET_STRING_OF_STATE(state)                  \
  case WebRTCPeerConnectionHandlerClient::k##state: \
    result = #state;                                \
    break;

// Note: All of these strings need to be kept in sync with
// peer_connection_update_table.js, in order to be displayed as friendly
// strings on chrome://webrtc-internals.

static const char* GetSignalingStateString(
    WebRTCPeerConnectionHandlerClient::SignalingState state) {
  const char* result = "";
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

static const char* GetIceConnectionStateString(
    WebRTCPeerConnectionHandlerClient::ICEConnectionState state) {
  const char* result = "";
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

static const char* GetIceGatheringStateString(
    WebRTCPeerConnectionHandlerClient::ICEGatheringState state) {
  const char* result = "";
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
// Note:
// The format must be consistent with what webrtc_internals.js expects.
// If you change it here, you must change webrtc_internals.js as well.
static std::unique_ptr<base::DictionaryValue> GetDictValueStats(
    const StatsReport& report) {
  if (report.values().empty())
    return NULL;

  auto values = base::MakeUnique<base::ListValue>();

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

  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetDouble("timestamp", report.timestamp());
  dict->Set("values", std::move(values));

  return dict;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
static std::unique_ptr<base::DictionaryValue> GetDictValue(
    const StatsReport& report) {
  std::unique_ptr<base::DictionaryValue> stats = GetDictValueStats(report);
  if (!stats)
    return NULL;

  // Note:
  // The format must be consistent with what webrtc_internals.js expects.
  // If you change it here, you must change webrtc_internals.js as well.
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->Set("stats", std::move(stats));
  result->SetString("id", report.id()->ToString());
  result->SetString("type", report.TypeToString());

  return result;
}

class InternalStatsObserver : public webrtc::StatsObserver {
 public:
  explicit InternalStatsObserver(int lid)
      : lid_(lid), main_thread_(base::ThreadTaskRunnerHandle::Get()) {}

  void OnComplete(const StatsReports& reports) override {
    std::unique_ptr<base::ListValue> list(new base::ListValue());

    for (const auto* r : reports) {
      std::unique_ptr<base::DictionaryValue> report = GetDictValue(*r);
      if (report)
        list->Append(std::move(report));
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
  static void OnCompleteImpl(std::unique_ptr<base::ListValue> list, int lid) {
    DCHECK(!list->empty());
    RenderThreadImpl::current()->Send(
        new PeerConnectionTrackerHost_AddStats(lid, *list.get()));
  }

  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
};

PeerConnectionTracker::PeerConnectionTracker()
    : next_local_id_(1), send_target_for_test_(nullptr) {}

PeerConnectionTracker::~PeerConnectionTracker() {
}

bool PeerConnectionTracker::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PeerConnectionTracker, message)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_GetAllStats, OnGetAllStats)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_OnSuspend, OnSuspend)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_StartEventLog, OnStartEventLog)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_StopEventLog, OnStopEventLog)
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
        observer, webrtc::PeerConnectionInterface::kStatsOutputLevelDebug,
        empty_track_id, blink::WebMediaStreamSource::kTypeAudio);
  }
}

RenderThread* PeerConnectionTracker::SendTarget() {
  if (send_target_for_test_) {
    return send_target_for_test_;
  }
  return RenderThreadImpl::current();
}

void PeerConnectionTracker::OnSuspend() {
  DCHECK(main_thread_.CalledOnValidThread());
  for (PeerConnectionIdMap::iterator it = peer_connection_id_map_.begin();
       it != peer_connection_id_map_.end(); ++it) {
    it->first->CloseClientPeerConnection();
  }
}

void PeerConnectionTracker::OnStartEventLog(int peer_connection_id,
                                            IPC::PlatformFileForTransit file) {
  DCHECK(main_thread_.CalledOnValidThread());
  for (auto& it : peer_connection_id_map_) {
    if (it.second == peer_connection_id) {
#if defined(OS_ANDROID)
      // A lower maximum filesize is used on Android because storage space is
      // more scarce on mobile. This upper limit applies to each peerconnection
      // individually, so the total amount of used storage can be a multiple of
      // this.
      const int64_t kMaxFilesizeBytes = 10000000;
#else
      const int64_t kMaxFilesizeBytes = 60000000;
#endif
      it.first->StartEventLog(file, kMaxFilesizeBytes);
      return;
    }
  }
}

void PeerConnectionTracker::OnStopEventLog(int peer_connection_id) {
  DCHECK(main_thread_.CalledOnValidThread());
  for (auto& it : peer_connection_id_map_) {
    if (it.second == peer_connection_id) {
      it.first->StopEventLog();
      return;
    }
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const blink::WebMediaConstraints& constraints,
    const blink::WebLocalFrame* frame) {
  DCHECK(main_thread_.CalledOnValidThread());
  DCHECK_EQ(GetLocalIDForHandler(pc_handler), -1);
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  PeerConnectionInfo info;

  info.lid = GetNextLocalID();
  info.rtc_configuration = SerializeConfiguration(config);

  info.constraints = SerializeMediaConstraints(constraints);
  if (frame)
    info.url = frame->GetDocument().Url().GetString().Utf8();
  else
    info.url = "test:testing";
  SendTarget()->Send(new PeerConnectionTrackerHost_AddPeerConnection(info));

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

  SendTarget()->Send(
      new PeerConnectionTrackerHost_RemovePeerConnection(it->second));

  peer_connection_id_map_.erase(it);
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebRTCOfferOptions& options) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "createOffer",
                           "options: {" + SerializeOfferOptions(options) + "}");
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaConstraints& constraints) {
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
    const blink::WebRTCAnswerOptions& options) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createAnswer", "options: {" + SerializeAnswerOptions(options) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaConstraints& constraints) {
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
  std::string value = "type: " + type + ", sdp: " + sdp;
  SendPeerConnectionUpdate(
      id,
      source == SOURCE_LOCAL ? "setLocalDescription" : "setRemoteDescription",
      value);
}

void PeerConnectionTracker::TrackSetConfiguration(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;

  SendPeerConnectionUpdate(id, "setConfiguration",
                           SerializeConfiguration(config));
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
  std::string value =
      "sdpMid: " + candidate.SdpMid().Utf8() + ", " +
      "sdpMLineIndex: " + base::UintToString(candidate.SdpMLineIndex()) + ", " +
      "candidate: " + candidate.Candidate().Utf8();

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
    Source source) {
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
  std::string value = "label: " + data_channel->label() + ", reliable: " +
                      SerializeBoolean(data_channel->reliable());
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
    RTCPeerConnectionHandler* pc_handler,
    Action action,
    const std::string& callback_type,
    const std::string& value) {
  DCHECK(main_thread_.CalledOnValidThread());
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  std::string update_type;
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
  SendPeerConnectionUpdate(id, "createDTMFSender", track.Id().Utf8());
}

void PeerConnectionTracker::TrackGetUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(main_thread_.CalledOnValidThread());

  SendTarget()->Send(new PeerConnectionTrackerHost_GetUserMedia(
      user_media_request.GetSecurityOrigin().ToString().Utf8(),
      user_media_request.Audio(), user_media_request.Video(),
      SerializeMediaConstraints(user_media_request.AudioConstraints()),
      SerializeMediaConstraints(user_media_request.VideoConstraints())));
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
  SendTarget()->Send(new PeerConnectionTrackerHost_UpdatePeerConnection(
      local_id, std::string(callback_type), value));
}

void PeerConnectionTracker::OverrideSendTargetForTesting(RenderThread* target) {
  send_target_for_test_ = target;
}

}  // namespace content
