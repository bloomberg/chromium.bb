// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/webrtc_native_handler.h"

#include <functional>

#include "base/logging.h"
#include "chrome/common/extensions/api/webrtc_cast_send_transport.h"
#include "chrome/common/extensions/api/webrtc_cast_udp_transport.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/media/cast_send_transport.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/base/host_port_pair.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"

using content::V8ValueConverter;

// Extension types.
using extensions::api::webrtc_cast_send_transport::CodecSpecificParams;
using extensions::api::webrtc_cast_send_transport::RtpCaps;
using extensions::api::webrtc_cast_send_transport::RtpParams;
using extensions::api::webrtc_cast_send_transport::RtpPayloadParams;
using extensions::api::webrtc_cast_udp_transport::UdpParams;

namespace extensions {

namespace {
const char kSendTransportNotFound[] = "The send transport cannot be found";
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

WebRtcNativeHandler::WebRtcNativeHandler(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context),
      last_transport_id_(0) {
  RouteFunction("CreateCastSendTransport",
      base::Bind(&WebRtcNativeHandler::CreateCastSendTransport,
                 base::Unretained(this)));
  RouteFunction("DestroyCastSendTransport",
      base::Bind(&WebRtcNativeHandler::DestroyCastSendTransport,
                 base::Unretained(this)));
  RouteFunction("GetCapsCastSendTransport",
      base::Bind(&WebRtcNativeHandler::GetCapsCastSendTransport,
                 base::Unretained(this)));
  RouteFunction("StartCastSendTransport",
      base::Bind(&WebRtcNativeHandler::StartCastSendTransport,
                 base::Unretained(this)));
  RouteFunction("StopCastSendTransport",
      base::Bind(&WebRtcNativeHandler::StopCastSendTransport,
                 base::Unretained(this)));
  RouteFunction("CreateCastUdpTransport",
      base::Bind(&WebRtcNativeHandler::CreateCastUdpTransport,
                 base::Unretained(this)));
  RouteFunction("DestroyCastUdpTransport",
      base::Bind(&WebRtcNativeHandler::DestroyCastUdpTransport,
                 base::Unretained(this)));
  RouteFunction("StartCastUdpTransport",
      base::Bind(&WebRtcNativeHandler::StartCastUdpTransport,
                 base::Unretained(this)));
}

WebRtcNativeHandler::~WebRtcNativeHandler() {
}

void WebRtcNativeHandler::CreateCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());
  CHECK(args[2]->IsFunction());

  const int inner_transport_id = args[0]->ToInt32()->Value();
  CastUdpTransport* inner_transport = GetUdpTransportOrThrow(
      inner_transport_id);
  if (!inner_transport)
    return;

  blink::WebDOMMediaStreamTrack track =
      blink::WebDOMMediaStreamTrack::fromV8Value(args[1]);
  if (track.isNull())
    return;
  const int transport_id = last_transport_id_++;
  send_transport_map_[transport_id] =
      linked_ptr<CastSendTransport>(
          new CastSendTransport(inner_transport, track.component()));

  v8::Handle<v8::Value> transport_id_v8 = v8::Integer::New(transport_id);
  context()->CallFunction(v8::Handle<v8::Function>::Cast(args[2]), 1,
                          &transport_id_v8);
}

void WebRtcNativeHandler::DestroyCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  if (!GetSendTransportOrThrow(transport_id))
    return;
  send_transport_map_.erase(transport_id);
}

void WebRtcNativeHandler::GetCapsCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  CastSendTransport* transport = GetSendTransportOrThrow(transport_id);
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

void WebRtcNativeHandler::StartCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0]->ToInt32()->Value();
  CastSendTransport* transport = GetSendTransportOrThrow(transport_id);
  if (!transport)
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<Value> params_value(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!params_value) {
    v8::ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertParams)));
    return;
  }
  scoped_ptr<RtpParams> params = RtpParams::FromValue(*params_value);
  if (!params) {
    v8::ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidRtpParams)));
    return;
  }

  CastRtpCaps cast_params;
  ToCastRtpParams(*params, &cast_params);
  transport->Start(cast_params);
}

void WebRtcNativeHandler::StopCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  CastSendTransport* transport = GetSendTransportOrThrow(transport_id);
  if (!transport)
    return;
  transport->Stop();
}

void WebRtcNativeHandler::CreateCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsFunction());

  const int transport_id = last_transport_id_++;
  udp_transport_map_[transport_id] =
      linked_ptr<CastUdpTransport>(new CastUdpTransport());

  v8::Handle<v8::Value> transport_id_v8 = v8::Integer::New(transport_id);
  context()->CallFunction(v8::Handle<v8::Function>::Cast(args[0]), 1,
                          &transport_id_v8);
}

void WebRtcNativeHandler::DestroyCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  if (!GetUdpTransportOrThrow(transport_id))
    return;
  udp_transport_map_.erase(transport_id);
}

void WebRtcNativeHandler::StartCastUdpTransport(
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
    v8::ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs)));
    return;
  }
  scoped_ptr<UdpParams> udp_params = UdpParams::FromValue(*udp_params_value);
  if (!udp_params) {
    v8::ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidUdpParams)));
    return;
  }
  transport->Start(net::HostPortPair(udp_params->address, udp_params->port));
}

CastSendTransport* WebRtcNativeHandler::GetSendTransportOrThrow(
    int transport_id) const {
  SendTransportMap::const_iterator iter = send_transport_map_.find(
      transport_id);
  if (iter != send_transport_map_.end())
    return iter->second.get();
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  v8::ThrowException(v8::Exception::RangeError(
      v8::String::NewFromUtf8(isolate, kSendTransportNotFound)));
  return NULL;
}

CastUdpTransport* WebRtcNativeHandler::GetUdpTransportOrThrow(
    int transport_id) const {
  UdpTransportMap::const_iterator iter = udp_transport_map_.find(
      transport_id);
  if (iter != udp_transport_map_.end())
    return iter->second.get();
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  v8::ThrowException(v8::Exception::RangeError(
      v8::String::NewFromUtf8(isolate, kUdpTransportNotFound)));
  return NULL;
}

}  // namespace extensions
