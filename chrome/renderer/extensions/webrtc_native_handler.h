// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_WEBRTC_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_WEBRTC_NATIVE_HANDLER_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "chrome/renderer/extensions/object_backed_native_handler.h"
#include "v8/include/v8.h"

class CastSendTransport;
class CastUdpTransport;

namespace extensions {

class ChromeV8Context;

// Native code that handle chrome.webrtc custom bindings.
class WebRtcNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit WebRtcNativeHandler(ChromeV8Context* context);
  virtual ~WebRtcNativeHandler();

 private:
  void CreateCastSendTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void DestroyCastSendTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void CreateParamsCastSendTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetCapsCastSendTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StartCastSendTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StopCastSendTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void CreateCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void DestroyCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StartCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StopCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the Send or UDP transport indexed by |transport_id|.
  // If not found, returns NULL and throws a V8 exception.
  CastSendTransport* GetSendTransportOrThrow(int transport_id) const;
  CastUdpTransport* GetUdpTransportOrThrow(int transport_id) const;

  int last_transport_id_;

  typedef std::map<int, linked_ptr<CastSendTransport> > SendTransportMap;
  SendTransportMap send_transport_map_;

  typedef std::map<int, linked_ptr<CastUdpTransport> > UdpTransportMap;
  UdpTransportMap udp_transport_map_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcNativeHandler);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_WEBRTC_NATIVE_HANDLER_H_
