// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/cast_streaming_native_handler.h"

#include <functional>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/extensions/api/cast_streaming_rtp_stream.h"
#include "chrome/common/extensions/api/cast_streaming_udp_transport.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/media/cast_rtp_stream.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/base/host_port_pair.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"

using content::V8ValueConverter;

// Extension types.
using extensions::api::cast_streaming_rtp_stream::CodecSpecificParams;
using extensions::api::cast_streaming_rtp_stream::RtpCaps;
using extensions::api::cast_streaming_rtp_stream::RtpParams;
using extensions::api::cast_streaming_rtp_stream::RtpPayloadParams;
using extensions::api::cast_streaming_udp_transport::UdpParams;

namespace extensions {

namespace {
const char kRtpStreamNotFound[] = "The RTP stream cannot be found";
const char kUdpTransportNotFound[] = "The UDP transport cannot be found";
const char kInvalidUdpParams[] = "Invalid UDP params";
const char kInvalidRtpParams[] = "Invalid value for RTP params";
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

void ToCastRtpPayloadParams(const RtpPayloadParams& ext_params,
                            CastRtpPayloadParams* cast_params) {
  cast_params->payload_type = ext_params.payload_type;
  cast_params->codec_name = ext_params.codec_name;
  cast_params->ssrc = ext_params.ssrc ? *ext_params.ssrc : 0;
  cast_params->clock_rate = ext_params.clock_rate ? *ext_params.clock_rate : 0;
  cast_params->min_bitrate =
      ext_params.min_bitrate ? *ext_params.min_bitrate : 0;
  cast_params->max_bitrate =
      ext_params.max_bitrate ? *ext_params.max_bitrate : 0;
  cast_params->channels = ext_params.channels ? *ext_params.channels : 0;
  cast_params->width = ext_params.width ? *ext_params.width : 0;
  cast_params->height = ext_params.height ? *ext_params.height : 0;
  for (size_t i = 0; i < ext_params.codec_specific_params.size(); ++i) {
    CastCodecSpecificParams cast_codec_params;
    ToCastCodecSpecificParams(*ext_params.codec_specific_params[i],
                              &cast_codec_params);
    cast_params->codec_specific_params.push_back(cast_codec_params);
  }
}

void FromCastRtpPayloadParams(const CastRtpPayloadParams& cast_params,
                              RtpPayloadParams* ext_params) {
  ext_params->payload_type = cast_params.payload_type;
  ext_params->codec_name = cast_params.codec_name;
  if (cast_params.ssrc)
    ext_params->ssrc.reset(new int(cast_params.ssrc));
  if (cast_params.clock_rate)
    ext_params->clock_rate.reset(new int(cast_params.clock_rate));
  if (cast_params.min_bitrate)
    ext_params->min_bitrate.reset(new int(cast_params.min_bitrate));
  if (cast_params.max_bitrate)
    ext_params->max_bitrate.reset(new int(cast_params.max_bitrate));
  if (cast_params.channels)
    ext_params->channels.reset(new int(cast_params.channels));
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

void FromCastRtpCaps(const CastRtpCaps& cast_caps, RtpCaps* ext_caps) {
  std::copy(cast_caps.rtcp_features.begin(), cast_caps.rtcp_features.end(),
            ext_caps->rtcp_features.begin());
  for (size_t i = 0; i < cast_caps.payloads.size(); ++i) {
    linked_ptr<RtpPayloadParams> ext_payload_params(new RtpPayloadParams());
    FromCastRtpPayloadParams(cast_caps.payloads[i], ext_payload_params.get());
    ext_caps->payloads.push_back(ext_payload_params);
  }
}

void ToCastRtpParams(const RtpParams& ext_params, CastRtpParams* cast_params) {
  std::copy(ext_params.rtcp_features.begin(), ext_params.rtcp_features.end(),
            cast_params->rtcp_features.begin());
  for (size_t i = 0; i < ext_params.payloads.size(); ++i) {
    CastRtpPayloadParams cast_payload_params;
    ToCastRtpPayloadParams(*ext_params.payloads[i], &cast_payload_params);
    cast_params->payloads.push_back(cast_payload_params);
  }
}

}  // namespace

CastStreamingNativeHandler::CastStreamingNativeHandler(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context),
      last_transport_id_(0),
      weak_factory_(this) {
  RouteFunction("CreateSession",
      base::Bind(&CastStreamingNativeHandler::CreateCastSession,
                 base::Unretained(this)));
  RouteFunction("DestroyCastRtpStream",
      base::Bind(&CastStreamingNativeHandler::DestroyCastRtpStream,
                 base::Unretained(this)));
  RouteFunction("GetCapsCastRtpStream",
      base::Bind(&CastStreamingNativeHandler::GetCapsCastRtpStream,
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
  RouteFunction("StartCastUdpTransport",
      base::Bind(&CastStreamingNativeHandler::StartCastUdpTransport,
                 base::Unretained(this)));
}

CastStreamingNativeHandler::~CastStreamingNativeHandler() {
}

void CastStreamingNativeHandler::CreateCastSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsObject());
  CHECK(args[2]->IsFunction());

  blink::WebDOMMediaStreamTrack track1 =
      blink::WebDOMMediaStreamTrack::fromV8Value(args[0]);
  if (track1.isNull())
    return;
  blink::WebDOMMediaStreamTrack track2 =
      blink::WebDOMMediaStreamTrack::fromV8Value(args[1]);
  if (track2.isNull())
    return;

  scoped_refptr<CastSession> session(new CastSession());
  scoped_ptr<CastRtpStream> stream1(
      new CastRtpStream(track1.component(), session));
  scoped_ptr<CastRtpStream> stream2(
      new CastRtpStream(track2.component(), session));
  scoped_ptr<CastUdpTransport> udp_transport(
      new CastUdpTransport(session));

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
  v8::HandleScope handle_scope(context()->isolate());
  v8::Context::Scope context_scope(context()->v8_context());

  const int stream1_id = last_transport_id_++;
  rtp_stream_map_[stream1_id] =
      linked_ptr<CastRtpStream>(stream1.release());
  const int stream2_id = last_transport_id_++;
  rtp_stream_map_[stream2_id] =
      linked_ptr<CastRtpStream>(stream2.release());
  const int udp_id = last_transport_id_++;
  udp_transport_map_[udp_id] =
      linked_ptr<CastUdpTransport>(udp_transport.release());

  v8::Handle<v8::Value> callback_args[3];
  callback_args[0] = v8::Integer::New(stream1_id);
  callback_args[1] = v8::Integer::New(stream2_id);
  callback_args[2] = v8::Integer::New(udp_id);
  context()->CallFunction(create_callback_.NewHandle(context()->isolate()),
                          3, callback_args);
  create_callback_.reset();
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

void CastStreamingNativeHandler::GetCapsCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  CastRtpCaps cast_caps = transport->GetCaps();
  RtpCaps caps;
  FromCastRtpCaps(cast_caps, &caps);

  scoped_ptr<base::DictionaryValue> caps_value = caps.ToValue();
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  args.GetReturnValue().Set(converter->ToV8Value(
      caps_value.get(), context()->v8_context()));
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
  scoped_ptr<Value> params_value(
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

  CastRtpCaps cast_params;
  ToCastRtpParams(*params, &cast_params);
  transport->Start(cast_params);
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

void CastStreamingNativeHandler::StartCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0]->ToInt32()->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<Value> udp_params_value(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!udp_params_value) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs)));
    return;
  }
  scoped_ptr<UdpParams> udp_params = UdpParams::FromValue(*udp_params_value);
  if (!udp_params) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidUdpParams)));
    return;
  }
  net::IPAddressNumber ip;
  if (!net::ParseIPLiteralToNumber(udp_params->address, &ip)) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidUdpParams)));
    return;
  }
  transport->Start(net::IPEndPoint(ip, udp_params->port));
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
