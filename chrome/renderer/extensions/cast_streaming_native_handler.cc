// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/cast_streaming_native_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/extensions/api/cast_streaming_receiver_session.h"
#include "chrome/common/extensions/api/cast_streaming_rtp_stream.h"
#include "chrome/common/extensions/api/cast_streaming_udp_transport.h"
#include "chrome/renderer/media/cast_receiver_session.h"
#include "chrome/renderer/media/cast_rtp_stream.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/media_stream_utils.h"
#include "extensions/renderer/script_context.h"
#include "media/base/audio_parameters.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

using content::V8ValueConverter;

// Extension types.
using extensions::api::cast_streaming_receiver_session::RtpReceiverParams;
using extensions::api::cast_streaming_rtp_stream::CodecSpecificParams;
using extensions::api::cast_streaming_rtp_stream::RtpParams;
using extensions::api::cast_streaming_rtp_stream::RtpPayloadParams;
using extensions::api::cast_streaming_udp_transport::IPEndPoint;

namespace extensions {

namespace {
const char kInvalidAesIvMask[] = "Invalid value for AES IV mask";
const char kInvalidAesKey[] = "Invalid value for AES key";
const char kInvalidAudioParams[] = "Invalid audio params";
const char kInvalidDestination[] = "Invalid destination";
const char kInvalidFPS[] = "Invalid FPS";
const char kInvalidMediaStreamURL[] = "Invalid MediaStream URL";
const char kInvalidRtpParams[] = "Invalid value for RTP params";
const char kInvalidLatency[] = "Invalid value for max_latency. (0-1000)";
const char kInvalidRtpTimebase[] = "Invalid rtp_timebase. (1000-1000000)";
const char kInvalidStreamArgs[] = "Invalid stream arguments";
const char kRtpStreamNotFound[] = "The RTP stream cannot be found";
const char kUdpTransportNotFound[] = "The UDP transport cannot be found";
const char kUnableToConvertArgs[] = "Unable to convert arguments";
const char kUnableToConvertParams[] = "Unable to convert params";

// These helper methods are used to convert between Extension API
// types and Cast types.
void ToCastCodecSpecificParams(const CodecSpecificParams& ext_params,
                               CastCodecSpecificParams* cast_params) {
  cast_params->key = ext_params.key;
  cast_params->value = ext_params.value;
}

void FromCastCodecSpecificParams(const CastCodecSpecificParams& cast_params,
                                 CodecSpecificParams* ext_params) {
  ext_params->key = cast_params.key;
  ext_params->value = cast_params.value;
}

namespace {
bool HexDecode(const std::string& input, std::string* output) {
  std::vector<uint8_t> bytes;
  if (!base::HexStringToBytes(input, &bytes))
    return false;
  output->assign(reinterpret_cast<const char*>(&bytes[0]), bytes.size());
  return true;
}
}  // namespace

bool ToCastRtpPayloadParamsOrThrow(v8::Isolate* isolate,
                                   const RtpPayloadParams& ext_params,
                                   CastRtpPayloadParams* cast_params) {
  cast_params->payload_type = ext_params.payload_type;
  cast_params->max_latency_ms = ext_params.max_latency;
  cast_params->min_latency_ms =
      ext_params.min_latency ? *ext_params.min_latency : ext_params.max_latency;
  cast_params->animated_latency_ms = ext_params.animated_latency
                                         ? *ext_params.animated_latency
                                         : ext_params.max_latency;
  cast_params->codec_name = ext_params.codec_name;
  cast_params->ssrc = ext_params.ssrc;
  cast_params->feedback_ssrc = ext_params.feedback_ssrc;
  cast_params->clock_rate = ext_params.clock_rate ? *ext_params.clock_rate : 0;
  cast_params->min_bitrate =
      ext_params.min_bitrate ? *ext_params.min_bitrate : 0;
  cast_params->max_bitrate =
      ext_params.max_bitrate ? *ext_params.max_bitrate : 0;
  cast_params->channels = ext_params.channels ? *ext_params.channels : 0;
  cast_params->max_frame_rate =
      ext_params.max_frame_rate ? *ext_params.max_frame_rate : 0.0;
  if (ext_params.aes_key &&
      !HexDecode(*ext_params.aes_key, &cast_params->aes_key)) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidAesKey)));
    return false;
  }
  if (ext_params.aes_iv_mask &&
      !HexDecode(*ext_params.aes_iv_mask, &cast_params->aes_iv_mask)) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidAesIvMask)));
    return false;
  }
  for (size_t i = 0; i < ext_params.codec_specific_params.size(); ++i) {
    CastCodecSpecificParams cast_codec_params;
    ToCastCodecSpecificParams(ext_params.codec_specific_params[i],
                              &cast_codec_params);
    cast_params->codec_specific_params.push_back(cast_codec_params);
  }
  return true;
}

void FromCastRtpPayloadParams(const CastRtpPayloadParams& cast_params,
                              RtpPayloadParams* ext_params) {
  ext_params->payload_type = cast_params.payload_type;
  ext_params->max_latency = cast_params.max_latency_ms;
  ext_params->min_latency.reset(new int(cast_params.min_latency_ms));
  ext_params->animated_latency.reset(new int(cast_params.animated_latency_ms));
  ext_params->codec_name = cast_params.codec_name;
  ext_params->ssrc = cast_params.ssrc;
  ext_params->feedback_ssrc = cast_params.feedback_ssrc;
  if (cast_params.clock_rate)
    ext_params->clock_rate.reset(new int(cast_params.clock_rate));
  if (cast_params.min_bitrate)
    ext_params->min_bitrate.reset(new int(cast_params.min_bitrate));
  if (cast_params.max_bitrate)
    ext_params->max_bitrate.reset(new int(cast_params.max_bitrate));
  if (cast_params.channels)
    ext_params->channels.reset(new int(cast_params.channels));
  if (cast_params.max_frame_rate > 0.0)
    ext_params->max_frame_rate.reset(new double(cast_params.max_frame_rate));
  for (size_t i = 0; i < cast_params.codec_specific_params.size(); ++i) {
    CodecSpecificParams ext_codec_params;
    FromCastCodecSpecificParams(cast_params.codec_specific_params[i],
                                &ext_codec_params);
    ext_params->codec_specific_params.push_back(std::move(ext_codec_params));
  }
}

void FromCastRtpParams(const CastRtpParams& cast_params,
                       RtpParams* ext_params) {
  std::copy(cast_params.rtcp_features.begin(),
            cast_params.rtcp_features.end(),
            std::back_inserter(ext_params->rtcp_features));
  FromCastRtpPayloadParams(cast_params.payload, &ext_params->payload);
}

bool ToCastRtpParamsOrThrow(v8::Isolate* isolate,
                            const RtpParams& ext_params,
                            CastRtpParams* cast_params) {
  std::copy(ext_params.rtcp_features.begin(),
            ext_params.rtcp_features.end(),
            std::back_inserter(cast_params->rtcp_features));
  if (!ToCastRtpPayloadParamsOrThrow(isolate,
                                     ext_params.payload,
                                     &cast_params->payload)) {
    return false;
  }
  return true;
}

}  // namespace

CastStreamingNativeHandler::CastStreamingNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      last_transport_id_(1),
      weak_factory_(this) {
  RouteFunction("CreateSession", "cast.streaming.session",
                base::Bind(&CastStreamingNativeHandler::CreateCastSession,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("DestroyCastRtpStream", "cast.streaming.rtpStream",
                base::Bind(&CastStreamingNativeHandler::DestroyCastRtpStream,
                           weak_factory_.GetWeakPtr()));
  RouteFunction(
      "GetSupportedParamsCastRtpStream", "cast.streaming.rtpStream",
      base::Bind(&CastStreamingNativeHandler::GetSupportedParamsCastRtpStream,
                 weak_factory_.GetWeakPtr()));
  RouteFunction("StartCastRtpStream", "cast.streaming.rtpStream",
                base::Bind(&CastStreamingNativeHandler::StartCastRtpStream,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("StopCastRtpStream", "cast.streaming.rtpStream",
                base::Bind(&CastStreamingNativeHandler::StopCastRtpStream,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("DestroyCastUdpTransport", "cast.streaming.udpTransport",
                base::Bind(&CastStreamingNativeHandler::DestroyCastUdpTransport,
                           weak_factory_.GetWeakPtr()));
  RouteFunction(
      "SetDestinationCastUdpTransport", "cast.streaming.udpTransport",
      base::Bind(&CastStreamingNativeHandler::SetDestinationCastUdpTransport,
                 weak_factory_.GetWeakPtr()));
  RouteFunction(
      "SetOptionsCastUdpTransport", "cast.streaming.udpTransport",
      base::Bind(&CastStreamingNativeHandler::SetOptionsCastUdpTransport,
                 weak_factory_.GetWeakPtr()));
  RouteFunction("ToggleLogging", "cast.streaming.rtpStream",
                base::Bind(&CastStreamingNativeHandler::ToggleLogging,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("GetRawEvents", "cast.streaming.rtpStream",
                base::Bind(&CastStreamingNativeHandler::GetRawEvents,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("GetStats", "cast.streaming.rtpStream",
                base::Bind(&CastStreamingNativeHandler::GetStats,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("StartCastRtpReceiver", "cast.streaming.receiverSession",
                base::Bind(&CastStreamingNativeHandler::StartCastRtpReceiver,
                           weak_factory_.GetWeakPtr()));
}

CastStreamingNativeHandler::~CastStreamingNativeHandler() {
  // Note: A superclass's destructor will call Invalidate(), but Invalidate()
  // may also be called at any time before destruction.
}

void CastStreamingNativeHandler::Invalidate() {
  // Cancel all function call routing and callbacks.
  weak_factory_.InvalidateWeakPtrs();

  // Clear all references to V8 and Cast objects, which will trigger
  // auto-destructions (effectively stopping all sessions).
  get_stats_callbacks_.clear();
  get_raw_events_callbacks_.clear();
  create_callback_.Reset();
  udp_transport_map_.clear();
  rtp_stream_map_.clear();

  ObjectBackedNativeHandler::Invalidate();
}

void CastStreamingNativeHandler::CreateCastSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[2]->IsFunction());

  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  if ((args[0]->IsNull() || args[0]->IsUndefined()) &&
      (args[1]->IsNull() || args[1]->IsUndefined())) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidStreamArgs)));
    return;
  }

  scoped_refptr<CastSession> session(new CastSession());
  std::unique_ptr<CastRtpStream> stream1, stream2;
  if (!args[0]->IsNull() && !args[0]->IsUndefined()) {
    CHECK(args[0]->IsObject());
    blink::WebDOMMediaStreamTrack track =
        blink::WebDOMMediaStreamTrack::fromV8Value(args[0]);
    if (track.isNull()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidStreamArgs)));
      return;
    }
    stream1.reset(new CastRtpStream(track.component(), session));
  }
  if (!args[1]->IsNull() && !args[1]->IsUndefined()) {
    CHECK(args[1]->IsObject());
    blink::WebDOMMediaStreamTrack track =
        blink::WebDOMMediaStreamTrack::fromV8Value(args[1]);
    if (track.isNull()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidStreamArgs)));
      return;
    }
    stream2.reset(new CastRtpStream(track.component(), session));
  }
  std::unique_ptr<CastUdpTransport> udp_transport(
      new CastUdpTransport(session));

  create_callback_.Reset(isolate, args[2].As<v8::Function>());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&CastStreamingNativeHandler::CallCreateCallback,
                 weak_factory_.GetWeakPtr(), base::Passed(&stream1),
                 base::Passed(&stream2), base::Passed(&udp_transport)));
}

void CastStreamingNativeHandler::CallCreateCallback(
    std::unique_ptr<CastRtpStream> stream1,
    std::unique_ptr<CastRtpStream> stream2,
    std::unique_ptr<CastUdpTransport> udp_transport) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  v8::Local<v8::Value> callback_args[3];
  callback_args[0] = v8::Null(isolate);
  callback_args[1] = v8::Null(isolate);

  if (stream1) {
    const int stream1_id = last_transport_id_++;
    callback_args[0] = v8::Integer::New(isolate, stream1_id);
    rtp_stream_map_[stream1_id] =
        linked_ptr<CastRtpStream>(stream1.release());
  }
  if (stream2) {
    const int stream2_id = last_transport_id_++;
    callback_args[1] = v8::Integer::New(isolate, stream2_id);
    rtp_stream_map_[stream2_id] =
        linked_ptr<CastRtpStream>(stream2.release());
  }
  const int udp_id = last_transport_id_++;
  udp_transport_map_[udp_id] =
      linked_ptr<CastUdpTransport>(udp_transport.release());
  callback_args[2] = v8::Integer::New(isolate, udp_id);
  context()->CallFunction(
      v8::Local<v8::Function>::New(isolate, create_callback_), 3,
      callback_args);
  create_callback_.Reset();
}

void CastStreamingNativeHandler::CallStartCallback(int stream_id) const {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> event_args = v8::Array::New(isolate, 1);
  event_args->Set(0, v8::Integer::New(isolate, stream_id));
  context()->DispatchEvent("cast.streaming.rtpStream.onStarted", event_args);
}

void CastStreamingNativeHandler::CallStopCallback(int stream_id) const {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> event_args = v8::Array::New(isolate, 1);
  event_args->Set(0, v8::Integer::New(isolate, stream_id));
  context()->DispatchEvent("cast.streaming.rtpStream.onStopped", event_args);
}

void CastStreamingNativeHandler::CallErrorCallback(
    int stream_id,
    const std::string& message) const {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> event_args = v8::Array::New(isolate, 2);
  event_args->Set(0, v8::Integer::New(isolate, stream_id));
  event_args->Set(
      1,
      v8::String::NewFromUtf8(
          isolate, message.data(), v8::String::kNormalString, message.size()));
  context()->DispatchEvent("cast.streaming.rtpStream.onError", event_args);
}

void CastStreamingNativeHandler::DestroyCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  if (!GetRtpStreamOrThrow(transport_id))
    return;
  rtp_stream_map_.erase(transport_id);
}

void CastStreamingNativeHandler::GetSupportedParamsCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  std::vector<CastRtpParams> cast_params = transport->GetSupportedParams();
  v8::Local<v8::Array> result =
      v8::Array::New(args.GetIsolate(),
                     static_cast<int>(cast_params.size()));
  for (size_t i = 0; i < cast_params.size(); ++i) {
    RtpParams params;
    FromCastRtpParams(cast_params[i], &params);
    std::unique_ptr<base::DictionaryValue> params_value = params.ToValue();
    result->Set(
        static_cast<int>(i),
        converter->ToV8Value(params_value.get(), context()->v8_context()));
  }
  args.GetReturnValue().Set(result);
}

void CastStreamingNativeHandler::StartCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  std::unique_ptr<base::Value> params_value(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!params_value) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertParams)));
    return;
  }
  std::unique_ptr<RtpParams> params = RtpParams::FromValue(*params_value);
  if (!params) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidRtpParams)));
    return;
  }

  CastRtpParams cast_params;
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  if (!ToCastRtpParamsOrThrow(isolate, *params, &cast_params))
    return;

  base::Closure start_callback =
      base::Bind(&CastStreamingNativeHandler::CallStartCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id);
  base::Closure stop_callback =
      base::Bind(&CastStreamingNativeHandler::CallStopCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id);
  CastRtpStream::ErrorCallback error_callback =
      base::Bind(&CastStreamingNativeHandler::CallErrorCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id);
  transport->Start(cast_params, start_callback, stop_callback, error_callback);
}

void CastStreamingNativeHandler::StopCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;
  transport->Stop();
}

void CastStreamingNativeHandler::DestroyCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  if (!GetUdpTransportOrThrow(transport_id))
    return;
  udp_transport_map_.erase(transport_id);
}

void CastStreamingNativeHandler::SetDestinationCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;

  net::IPEndPoint dest;
  if (!IPEndPointFromArg(args.GetIsolate(),
                         args[1],
                         &dest)) {
    return;
  }
  transport->SetDestination(
      dest,
      base::Bind(&CastStreamingNativeHandler::CallErrorCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id));
}

void CastStreamingNativeHandler::SetOptionsCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;

  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  base::Value* options_value =
      converter->FromV8Value(args[1], context()->v8_context());
  base::DictionaryValue* options;
  if (!options_value || !options_value->GetAsDictionary(&options)) {
    delete options_value;
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs)));
    return;
  }
  transport->SetOptions(base::WrapUnique(options));
}

void CastStreamingNativeHandler::ToggleLogging(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());

  const int stream_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastRtpStream* stream = GetRtpStreamOrThrow(stream_id);
  if (!stream)
    return;

  const bool enable = args[1]->ToBoolean(args.GetIsolate())->Value();
  stream->ToggleLogging(enable);
}

void CastStreamingNativeHandler::GetRawEvents(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsNull() || args[1]->IsString());
  CHECK(args[2]->IsFunction());

  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  linked_ptr<v8::Global<v8::Function>> callback(new v8::Global<v8::Function>(
      args.GetIsolate(), args[2].As<v8::Function>()));
  std::string extra_data;
  if (!args[1]->IsNull()) {
    extra_data = *v8::String::Utf8Value(args[1]);
  }

  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  get_raw_events_callbacks_.insert(std::make_pair(transport_id, callback));

  transport->GetRawEvents(
      base::Bind(&CastStreamingNativeHandler::CallGetRawEventsCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id),
      extra_data);
}

void CastStreamingNativeHandler::GetStats(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());
  const int transport_id = args[0]->ToInt32(args.GetIsolate())->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  linked_ptr<v8::Global<v8::Function>> callback(new v8::Global<v8::Function>(
      args.GetIsolate(), args[1].As<v8::Function>()));
  get_stats_callbacks_.insert(std::make_pair(transport_id, callback));

  transport->GetStats(
      base::Bind(&CastStreamingNativeHandler::CallGetStatsCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id));
}

void CastStreamingNativeHandler::CallGetRawEventsCallback(
    int transport_id,
    std::unique_ptr<base::BinaryValue> raw_events) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  RtpStreamCallbackMap::iterator it =
      get_raw_events_callbacks_.find(transport_id);
  if (it == get_raw_events_callbacks_.end())
    return;
  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Local<v8::Value> callback_args[] = {
      converter->ToV8Value(raw_events.get(), context()->v8_context())};
  context()->CallFunction(v8::Local<v8::Function>::New(isolate, *it->second),
                          arraysize(callback_args), callback_args);
  get_raw_events_callbacks_.erase(it);
}

void CastStreamingNativeHandler::CallGetStatsCallback(
    int transport_id,
    std::unique_ptr<base::DictionaryValue> stats) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  RtpStreamCallbackMap::iterator it = get_stats_callbacks_.find(transport_id);
  if (it == get_stats_callbacks_.end())
    return;

  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Local<v8::Value> callback_args[] = {
      converter->ToV8Value(stats.get(), context()->v8_context())};
  context()->CallFunction(v8::Local<v8::Function>::New(isolate, *it->second),
                          arraysize(callback_args), callback_args);
  get_stats_callbacks_.erase(it);
}

CastRtpStream* CastStreamingNativeHandler::GetRtpStreamOrThrow(
    int transport_id) const {
  RtpStreamMap::const_iterator iter = rtp_stream_map_.find(
      transport_id);
  if (iter != rtp_stream_map_.end())
    return iter->second.get();
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  isolate->ThrowException(v8::Exception::RangeError(v8::String::NewFromUtf8(
      isolate, kRtpStreamNotFound)));
  return NULL;
}

CastUdpTransport* CastStreamingNativeHandler::GetUdpTransportOrThrow(
    int transport_id) const {
  UdpTransportMap::const_iterator iter = udp_transport_map_.find(
      transport_id);
  if (iter != udp_transport_map_.end())
    return iter->second.get();
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  isolate->ThrowException(v8::Exception::RangeError(
      v8::String::NewFromUtf8(isolate, kUdpTransportNotFound)));
  return NULL;
}

bool CastStreamingNativeHandler::FrameReceiverConfigFromArg(
    v8::Isolate* isolate,
    const v8::Local<v8::Value>& arg,
    media::cast::FrameReceiverConfig* config) const {
  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  std::unique_ptr<base::Value> params_value(
      converter->FromV8Value(arg, context()->v8_context()));
  if (!params_value) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kUnableToConvertParams)));
    return false;
  }
  std::unique_ptr<RtpReceiverParams> params =
      RtpReceiverParams::FromValue(*params_value);
  if (!params) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidRtpParams)));
    return false;
  }

  config->receiver_ssrc = params->receiver_ssrc;
  config->sender_ssrc = params->sender_ssrc;
  config->rtp_max_delay_ms = params->max_latency;
  if (config->rtp_max_delay_ms < 0 || config->rtp_max_delay_ms > 1000) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidLatency)));
    return false;
  }
  config->channels = 2;
  if (params->codec_name == "OPUS") {
    config->codec = media::cast::CODEC_AUDIO_OPUS;
    config->rtp_timebase = 48000;
    config->rtp_payload_type = media::cast::kDefaultRtpAudioPayloadType;
  } else if (params->codec_name == "PCM16") {
    config->codec = media::cast::CODEC_AUDIO_PCM16;
    config->rtp_timebase = 48000;
    config->rtp_payload_type = media::cast::kDefaultRtpAudioPayloadType;
  } else if (params->codec_name == "AAC") {
    config->codec = media::cast::CODEC_AUDIO_AAC;
    config->rtp_timebase = 48000;
    config->rtp_payload_type = media::cast::kDefaultRtpAudioPayloadType;
  } else if (params->codec_name == "VP8") {
    config->codec = media::cast::CODEC_VIDEO_VP8;
    config->rtp_timebase = 90000;
    config->rtp_payload_type = media::cast::kDefaultRtpVideoPayloadType;
  } else if (params->codec_name == "H264") {
    config->codec = media::cast::CODEC_VIDEO_H264;
    config->rtp_timebase = 90000;
    config->rtp_payload_type = media::cast::kDefaultRtpVideoPayloadType;
  }
  if (params->rtp_timebase) {
    config->rtp_timebase = *params->rtp_timebase;
    if (config->rtp_timebase < 1000 || config->rtp_timebase > 1000000) {
      isolate->ThrowException(v8::Exception::TypeError(
          v8::String::NewFromUtf8(isolate, kInvalidRtpTimebase)));
      return false;
    }
  }
  if (params->aes_key &&
      !HexDecode(*params->aes_key, &config->aes_key)) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidAesKey)));
    return false;
  }
  if (params->aes_iv_mask &&
      !HexDecode(*params->aes_iv_mask, &config->aes_iv_mask)) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidAesIvMask)));
    return false;
  }
  return true;
}

bool CastStreamingNativeHandler::IPEndPointFromArg(
    v8::Isolate* isolate,
    const v8::Local<v8::Value>& arg,
    net::IPEndPoint* ip_endpoint) const {
  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  std::unique_ptr<base::Value> destination_value(
      converter->FromV8Value(arg, context()->v8_context()));
  if (!destination_value) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidAesIvMask)));
    return false;
  }
  std::unique_ptr<IPEndPoint> destination =
      IPEndPoint::FromValue(*destination_value);
  if (!destination) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidDestination)));
    return false;
  }
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(destination->address)) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidDestination)));
    return false;
  }
  *ip_endpoint = net::IPEndPoint(ip, destination->port);
  return true;
}

void CastStreamingNativeHandler::StartCastRtpReceiver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 8 || args.Length() > 9 ||
      !args[0]->IsObject() ||
      !args[1]->IsObject() ||
      !args[2]->IsObject() ||
      !args[3]->IsInt32() ||
      !args[4]->IsInt32() ||
      !args[5]->IsNumber() ||
      !args[6]->IsString()) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs)));
    return;
  }

  v8::Isolate* isolate = context()->v8_context()->GetIsolate();

  scoped_refptr<CastReceiverSession> session(
      new CastReceiverSession());
  media::cast::FrameReceiverConfig audio_config;
  media::cast::FrameReceiverConfig video_config;
  net::IPEndPoint local_endpoint;
  net::IPEndPoint remote_endpoint;

  if (!FrameReceiverConfigFromArg(isolate, args[0], &audio_config) ||
      !FrameReceiverConfigFromArg(isolate, args[1], &video_config) ||
      !IPEndPointFromArg(isolate, args[2], &local_endpoint)) {
    return;
  }

  const int max_width = args[3]->ToInt32(args.GetIsolate())->Value();
  const int max_height = args[4]->ToInt32(args.GetIsolate())->Value();
  const double fps = args[5]->NumberValue();

  if (fps <= 1) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidFPS)));
    return;
  }

  const std::string url = *v8::String::Utf8Value(args[6]);
  blink::WebMediaStream stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));

  if (stream.isNull()) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidMediaStreamURL)));
    return;
  }

  media::VideoCaptureFormat capture_format(gfx::Size(max_width, max_height),
                                           fps, media::PIXEL_FORMAT_I420);

  video_config.target_frame_rate = fps;
  audio_config.target_frame_rate = 100;

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LINEAR,
      media::GuessChannelLayout(audio_config.channels),
      audio_config.rtp_timebase,  // sampling rate
      16, audio_config.rtp_timebase / audio_config.target_frame_rate);

  if (!params.IsValid()) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidAudioParams)));
    return;
  }

  std::unique_ptr<base::DictionaryValue> options;
  if (args.Length() >= 9) {
    std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    std::unique_ptr<base::Value> options_value(
        converter->FromV8Value(args[8], context()->v8_context()));
    if (!options_value->IsType(base::Value::TYPE_NULL)) {
      options = base::DictionaryValue::From(std::move(options_value));
      if (!options) {
        args.GetIsolate()->ThrowException(v8::Exception::TypeError(
            v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs)));
        return;
      }
    }
  }

  if (!options) {
    options.reset(new base::DictionaryValue());
  }

  v8::CopyablePersistentTraits<v8::Function>::CopyablePersistent error_callback;
  error_callback.Reset(args.GetIsolate(),
                       v8::Local<v8::Function>(args[7].As<v8::Function>()));

  session->Start(
      audio_config, video_config, local_endpoint, remote_endpoint,
      std::move(options), capture_format,
      base::Bind(&CastStreamingNativeHandler::AddTracksToMediaStream,
                 weak_factory_.GetWeakPtr(), url, params),
      base::Bind(&CastStreamingNativeHandler::CallReceiverErrorCallback,
                 weak_factory_.GetWeakPtr(), error_callback));
}

void CastStreamingNativeHandler::CallReceiverErrorCallback(
    v8::CopyablePersistentTraits<v8::Function>::CopyablePersistent function,
    const std::string& error_message) {
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  v8::Local<v8::Value> arg = v8::String::NewFromUtf8(isolate,
                                                      error_message.data(),
                                                      v8::String::kNormalString,
                                                      error_message.size());
  context()->CallFunction(
      v8::Local<v8::Function>::New(isolate, function), 1, &arg);
}

void CastStreamingNativeHandler::AddTracksToMediaStream(
    const std::string& url,
    const media::AudioParameters& params,
    scoped_refptr<media::AudioCapturerSource> audio,
    std::unique_ptr<media::VideoCapturerSource> video) {
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));
  if (web_stream.isNull()) {
    LOG(DFATAL) << "Stream not found.";
    return;
  }
  if (!content::AddAudioTrackToMediaStream(
          audio, params.sample_rate(), params.channel_layout(),
          params.frames_per_buffer(), true,  // is_remote
          true,                              // is_readonly
          &web_stream)) {
    LOG(ERROR) << "Failed to add Cast audio track to media stream.";
  }
  if (!content::AddVideoTrackToMediaStream(std::move(video), true,  // is_remote
                                           true,  // is_readonly
                                           &web_stream)) {
    LOG(ERROR) << "Failed to add Cast video track to media stream.";
  }
}

}  // namespace extensions
