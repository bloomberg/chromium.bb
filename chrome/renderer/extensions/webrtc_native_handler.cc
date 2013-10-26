// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/webrtc_native_handler.h"

#include "base/logging.h"

namespace extensions {

WebRtcNativeHandler::WebRtcNativeHandler(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context) {
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
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::DestroyCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::CreateParamsCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::GetCapsCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::StartCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::StopCastSendTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::CreateCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::StartCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

void WebRtcNativeHandler::StopCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
