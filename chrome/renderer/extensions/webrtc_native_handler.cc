// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/webrtc_native_handler.h"

#include "base/logging.h"
#include "chrome/common/extensions/api/webrtc_cast_send_transport.h"
#include "chrome/common/extensions/api/webrtc_cast_udp_transport.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/media/cast_send_transport.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/base/host_port_pair.h"

using content::V8ValueConverter;

// Extension types.
using extensions::api::webrtc_cast_send_transport::RtpCaps;
using extensions::api::webrtc_cast_send_transport::RtpParams;
using extensions::api::webrtc_cast_send_transport::RtpPayloadParams;
using extensions::api::webrtc_cast_udp_transport::CreateInfo;
using extensions::api::webrtc_cast_udp_transport::UdpParams;

namespace extensions {

namespace {
const char kSendTransportNotFound[] = "The send transport cannot be found";
const char kUdpTransportNotFound[] = "The UDP transport cannot be found";
const char kInvalidUdpParams[] = "Invalid UDP params";
const char kInvalidRtpCaps[] = "Invalid value for RTP caps";
const char kInvalidRtpParams[] = "Invalid value for RTP params";
const char kUnableToConvertArgs[] = "Unable to convert arguments";
const char kUnableToConvertCaps[] = "Unable to convert caps";
const char kUnableToConvertParams[] = "Unable to convert params";

// These helper methods are used to convert between Extension API
// types and Cast types.
bool ToCastRtpCaps(const RtpCaps& ext_caps, CastRtpCaps* cast_caps) {
  NOTIMPLEMENTED();
  return true;
}

bool FromCastRtpCaps(const CastRtpCaps& cast_caps, RtpCaps* ext_caps) {
  NOTIMPLEMENTED();
  return true;
}

bool ToCastRtpParams(const RtpParams& ext_params, CastRtpParams* cast_params) {
  NOTIMPLEMENTED();
  return true;
}

bool FromCastRtpParams(const CastRtpParams& cast_params,
                       RtpParams* ext_params) {
  NOTIMPLEMENTED();
  return true;
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
  RouteFunction("CreateParamsCastSendTransport",
      base::Bind(&WebRtcNativeHandler::CreateParamsCastSendTransport,
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
  RouteFunction("StopCastUdpTransport",
      base::Bind(&WebRtcNativeHandler::StopCastUdpTransport,
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

  // TODO(hclam): Convert second argument from v8::Value to
  // WebMediaStreamTrack.
  int transport_id = last_transport_id_++;
  send_transport_map_[transport_id] =
      linked_ptr<CastSendTransport>(new CastSendTransport(inner_transport));

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

void WebRtcNativeHandler::CreateParamsCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());
  CHECK(args[2]->IsFunction());

  const int transport_id = args[0]->ToInt32()->Value();
  CastSendTransport* transport = GetSendTransportOrThrow(transport_id);
  if (!transport)
    return;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<Value> remote_caps_value(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!remote_caps_value) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertArgs)));
    return;
  }
  scoped_ptr<RtpCaps> remote_caps =
      RtpCaps::FromValue(*remote_caps_value);
  if (!remote_caps) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kInvalidRtpCaps)));
    return;
  }

  CastRtpCaps cast_remote_caps;
  if (!ToCastRtpCaps(*remote_caps, &cast_remote_caps)) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertCaps)));
    return;
  }
  CastRtpParams cast_params = transport->CreateParams(cast_remote_caps);
  RtpParams params;
  if (!FromCastRtpParams(cast_params, &params)) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertParams)));
    return;
  }

  scoped_ptr<base::DictionaryValue> params_value = params.ToValue();
  v8::Handle<v8::Value> params_v8 = converter->ToV8Value(
      params_value.get(), context()->v8_context());
  context()->CallFunction(v8::Handle<v8::Function>::Cast(args[2]), 1,
                          &params_v8);
}

void WebRtcNativeHandler::GetCapsCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());

  const int transport_id = args[0]->ToInt32()->Value();
  CastSendTransport* transport = GetSendTransportOrThrow(transport_id);
  if (!transport)
    return;

  CastRtpCaps cast_caps = transport->GetCaps();
  RtpCaps caps;
  if (!FromCastRtpCaps(cast_caps, &caps)) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertCaps)));
    return;
  }

  scoped_ptr<base::DictionaryValue> caps_value = caps.ToValue();
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> caps_v8 = converter->ToV8Value(
      caps_value.get(), context()->v8_context());
  context()->CallFunction(v8::Handle<v8::Function>::Cast(args[1]), 1,
                          &caps_v8);
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
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertParams)));
    return;
  }
  scoped_ptr<RtpParams> params = RtpParams::FromValue(*params_value);
  if (!params) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kInvalidRtpParams)));
    return;
  }

  CastRtpCaps cast_params;
  if (!ToCastRtpParams(*params, &cast_params)) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertParams)));
    return;
  }
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

  // TODO(hclam): Fill in |create_info| properly.
  CreateInfo create_info;
  create_info.transport_id = last_transport_id_++;
  udp_transport_map_[create_info.transport_id] =
      linked_ptr<CastUdpTransport>(new CastUdpTransport());

  scoped_ptr<base::DictionaryValue> create_info_value = create_info.ToValue();
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> create_info_v8 = converter->ToV8Value(
      create_info_value.get(), context()->v8_context());
  context()->CallFunction(v8::Handle<v8::Function>::Cast(
      args[0]), 1, &create_info_v8);
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
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kUnableToConvertArgs)));
    return;
  }
  scoped_ptr<UdpParams> udp_params = UdpParams::FromValue(*udp_params_value);
  if (!udp_params) {
    v8::ThrowException(v8::Exception::TypeError(v8::String::New(
        kInvalidUdpParams)));
    return;
  }
  transport->Start(net::HostPortPair(udp_params->address, udp_params->port));
}

void WebRtcNativeHandler::StopCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0]->ToInt32()->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;
  transport->Stop();
}

CastSendTransport* WebRtcNativeHandler::GetSendTransportOrThrow(
    int transport_id) const {
  SendTransportMap::const_iterator iter = send_transport_map_.find(
      transport_id);
  if (iter != send_transport_map_.end())
    return iter->second.get();
  v8::ThrowException(v8::Exception::RangeError(v8::String::New(
      kSendTransportNotFound)));
  return NULL;
}

CastUdpTransport* WebRtcNativeHandler::GetUdpTransportOrThrow(
    int transport_id) const {
  UdpTransportMap::const_iterator iter = udp_transport_map_.find(
      transport_id);
  if (iter != udp_transport_map_.end())
    return iter->second.get();
  v8::ThrowException(v8::Exception::RangeError(v8::String::New(
      kUdpTransportNotFound)));
  return NULL;
}

}  // namespace extensions
