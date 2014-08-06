// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/cast_streaming_native_handler.h"

#include <functional>
#include <iterator>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/api/cast_streaming_rtp_stream.h"
#include "chrome/common/extensions/api/cast_streaming_udp_transport.h"
#include "chrome/renderer/media/cast_rtp_stream.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/script_context.h"
#include "net/base/host_port_pair.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"

using content::V8ValueConverter;

// Extension types.
using extensions::api::cast_streaming_rtp_stream::CodecSpecificParams;
using extensions::api::cast_streaming_rtp_stream::RtpParams;
using extensions::api::cast_streaming_rtp_stream::RtpPayloadParams;
using extensions::api::cast_streaming_udp_transport::IPEndPoint;

namespace extensions {

namespace {
const char kRtpStreamNotFound[] = "The RTP stream cannot be found";
const char kUdpTransportNotFound[] = "The UDP transport cannot be found";
const char kInvalidDestination[] = "Invalid destination";
const char kInvalidRtpParams[] = "Invalid value for RTP params";
const char kInvalidAesKey[] = "Invalid value for AES key";
const char kInvalidAesIvMask[] = "Invalid value for AES IV mask";
const char kInvalidStreamArgs[] = "Invalid stream arguments";
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
  std::vector<uint8> bytes;
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
  cast_params->width = ext_params.width ? *ext_params.width : 0;
  cast_params->height = ext_params.height ? *ext_params.height : 0;
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
    ToCastCodecSpecificParams(*ext_params.codec_specific_params[i],
                              &cast_codec_params);
    cast_params->codec_specific_params.push_back(cast_codec_params);
  }
  return true;
}

void FromCastRtpPayloadParams(const CastRtpPayloadParams& cast_params,
                              RtpPayloadParams* ext_params) {
  ext_params->payload_type = cast_params.payload_type;
  ext_params->max_latency = cast_params.max_latency_ms;
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
  if (cast_params.width)
    ext_params->width.reset(new int(cast_params.width));
  if (cast_params.height)
    ext_params->height.reset(new int(cast_params.height));
  for (size_t i = 0; i < cast_params.codec_specific_params.size(); ++i) {
    linked_ptr<CodecSpecificParams> ext_codec_params(
        new CodecSpecificParams());
    FromCastCodecSpecificParams(cast_params.codec_specific_params[i],
                                ext_codec_params.get());
    ext_params->codec_specific_params.push_back(ext_codec_params);
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
  RouteFunction("CreateSession",
      base::Bind(&CastStreamingNativeHandler::CreateCastSession,
                 base::Unretained(this)));
  RouteFunction("DestroyCastRtpStream",
      base::Bind(&CastStreamingNativeHandler::DestroyCastRtpStream,
                 base::Unretained(this)));
  RouteFunction("GetSupportedParamsCastRtpStream",
      base::Bind(&CastStreamingNativeHandler::GetSupportedParamsCastRtpStream,
                 base::Unretained(this)));
  RouteFunction("StartCastRtpStream",
      base::Bind(&CastStreamingNativeHandler::StartCastRtpStream,
                 base::Unretained(this)));
  RouteFunction("StopCastRtpStream",
      base::Bind(&CastStreamingNativeHandler::StopCastRtpStream,
                 base::Unretained(this)));
  RouteFunction("DestroyCastUdpTransport",
      base::Bind(&CastStreamingNativeHandler::DestroyCastUdpTransport,
                 base::Unretained(this)));
  RouteFunction("SetDestinationCastUdpTransport",
      base::Bind(&CastStreamingNativeHandler::SetDestinationCastUdpTransport,
                 base::Unretained(this)));
  RouteFunction("ToggleLogging",
                base::Bind(&CastStreamingNativeHandler::ToggleLogging,
                           base::Unretained(this)));
  RouteFunction("GetRawEvents",
                base::Bind(&CastStreamingNativeHandler::GetRawEvents,
                           base::Unretained(this)));
  RouteFunction("GetStats",
                base::Bind(&CastStreamingNativeHandler::GetStats,
                           base::Unretained(this)));
}

CastStreamingNativeHandler::~CastStreamingNativeHandler() {
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
  scoped_ptr<CastRtpStream> stream1, stream2;
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
  scoped_ptr<CastUdpTransport> udp_transport(
      new CastUdpTransport(session));

  // TODO(imcheng): Use a weak reference to ensure we don't call into an
  // invalid context when the callback is invoked.
  create_callback_.reset(args[2].As<v8::Function>());

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &CastStreamingNativeHandler::CallCreateCallback,
          weak_factory_.GetWeakPtr(),
          base::Passed(&stream1),
          base::Passed(&stream2),
          base::Passed(&udp_transport)));
}

void CastStreamingNativeHandler::CallCreateCallback(
    scoped_ptr<CastRtpStream> stream1,
    scoped_ptr<CastRtpStream> stream2,
    scoped_ptr<CastUdpTransport> udp_transport) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  v8::Handle<v8::Value> callback_args[3];
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
  context()->CallFunction(create_callback_.NewHandle(isolate),
                          3, callback_args);
  create_callback_.reset();
}

void CastStreamingNativeHandler::CallStartCallback(int stream_id) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Array> event_args = v8::Array::New(isolate, 1);
  event_args->Set(0, v8::Integer::New(isolate, stream_id));
  context()->DispatchEvent("cast.streaming.rtpStream.onStarted", event_args);
}

void CastStreamingNativeHandler::CallStopCallback(int stream_id) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Array> event_args = v8::Array::New(isolate, 1);
  event_args->Set(0, v8::Integer::New(isolate, stream_id));
  context()->DispatchEvent("cast.streaming.rtpStream.onStopped", event_args);
}

void CastStreamingNativeHandler::CallErrorCallback(int stream_id,
                                                   const std::string& message) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Array> event_args = v8::Array::New(isolate, 2);
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

  const int transport_id = args[0]->ToInt32()->Value();
  if (!GetRtpStreamOrThrow(transport_id))
    return;
  rtp_stream_map_.erase(transport_id);
}

void CastStreamingNativeHandler::GetSupportedParamsCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  std::vector<CastRtpParams> cast_params = transport->GetSupportedParams();
  v8::Handle<v8::Array> result =
      v8::Array::New(args.GetIsolate(),
                     static_cast<int>(cast_params.size()));
  for (size_t i = 0; i < cast_params.size(); ++i) {
    RtpParams params;
    FromCastRtpParams(cast_params[i], &params);
    scoped_ptr<base::DictionaryValue> params_value = params.ToValue();
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

  const int transport_id = args[0]->ToInt32()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<base::Value> params_value(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!params_value) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertParams)));
    return;
  }
  scoped_ptr<RtpParams> params = RtpParams::FromValue(*params_value);
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

  const int transport_id = args[0]->ToInt32()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;
  transport->Stop();
}

void CastStreamingNativeHandler::DestroyCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  if (!GetUdpTransportOrThrow(transport_id))
    return;
  udp_transport_map_.erase(transport_id);
}

void CastStreamingNativeHandler::SetDestinationCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0]->ToInt32()->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<base::Value> destination_value(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!destination_value) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs)));
    return;
  }
  scoped_ptr<IPEndPoint> destination =
      IPEndPoint::FromValue(*destination_value);
  if (!destination) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidDestination)));
    return;
  }
  net::IPAddressNumber ip;
  if (!net::ParseIPLiteralToNumber(destination->address, &ip)) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidDestination)));
    return;
  }
  transport->SetDestination(net::IPEndPoint(ip, destination->port));
}

void CastStreamingNativeHandler::ToggleLogging(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());

  const int stream_id = args[0]->ToInt32()->Value();
  CastRtpStream* stream = GetRtpStreamOrThrow(stream_id);
  if (!stream)
    return;

  const bool enable = args[1]->ToBoolean()->Value();
  stream->ToggleLogging(enable);
}

void CastStreamingNativeHandler::GetRawEvents(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsNull() || args[1]->IsString());
  CHECK(args[2]->IsFunction());

  const int transport_id = args[0]->ToInt32()->Value();
  // TODO(imcheng): Use a weak reference to ensure we don't call into an
  // invalid context when the callback is invoked.
  linked_ptr<ScopedPersistent<v8::Function> > callback(
      new ScopedPersistent<v8::Function>(args[2].As<v8::Function>()));
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
  const int transport_id = args[0]->ToInt32()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  // TODO(imcheng): Use a weak reference to ensure we don't call into an
  // invalid context when the callback is invoked.
  linked_ptr<ScopedPersistent<v8::Function> > callback(
      new ScopedPersistent<v8::Function>(args[1].As<v8::Function>()));
  get_stats_callbacks_.insert(std::make_pair(transport_id, callback));

  transport->GetStats(
      base::Bind(&CastStreamingNativeHandler::CallGetStatsCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id));
}

void CastStreamingNativeHandler::CallGetRawEventsCallback(
    int transport_id,
    scoped_ptr<base::BinaryValue> raw_events) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  RtpStreamCallbackMap::iterator it =
      get_raw_events_callbacks_.find(transport_id);
  if (it == get_raw_events_callbacks_.end())
    return;
  v8::Handle<v8::Value> callback_args[1];
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  callback_args[0] =
      converter->ToV8Value(raw_events.get(), context()->v8_context());
  context()->CallFunction(it->second->NewHandle(isolate), 1, callback_args);
  get_raw_events_callbacks_.erase(it);
}

void CastStreamingNativeHandler::CallGetStatsCallback(
    int transport_id,
    scoped_ptr<base::DictionaryValue> stats) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  RtpStreamCallbackMap::iterator it = get_stats_callbacks_.find(transport_id);
  if (it == get_stats_callbacks_.end())
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> callback_args[1];
  callback_args[0] = converter->ToV8Value(stats.get(), context()->v8_context());
  context()->CallFunction(it->second->NewHandle(isolate), 1, callback_args);
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

}  // namespace extensions
