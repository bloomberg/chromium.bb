// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CAST_STREAMING_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_CAST_STREAMING_NATIVE_HANDLER_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/scoped_persistent.h"
#include "v8/include/v8.h"

class CastRtpStream;
class CastUdpTransport;

namespace base {
class BinaryValue;
class DictionaryValue;
}

namespace blink {
class WebMediaStream;
}

namespace net {
class IPEndPoint;
}

namespace media {
class AudioCapturerSource;
class AudioParameters;
class VideoCapturerSource;
namespace cast {
struct FrameReceiverConfig;
}
}

namespace extensions {

// Native code that handle chrome.webrtc custom bindings.
class CastStreamingNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit CastStreamingNativeHandler(ScriptContext* context);
  ~CastStreamingNativeHandler() override;

 private:
  void CreateCastSession(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void DestroyCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void CreateParamsCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetSupportedParamsCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StartCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StopCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void DestroyCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void SetDestinationCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void SetOptionsCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StopCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StartCastRtpReceiver(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void ToggleLogging(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetRawEvents(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetStats(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Helper method to call the v8 callback function after a session is
  // created.
  void CallCreateCallback(scoped_ptr<CastRtpStream> stream1,
                          scoped_ptr<CastRtpStream> stream2,
                          scoped_ptr<CastUdpTransport> udp_transport);

  void CallStartCallback(int stream_id);
  void CallStopCallback(int stream_id);
  void CallErrorCallback(int stream_id, const std::string& message);

  // Callback called after a cast receiver has been started. Adds the
  // output audio/video streams to the MediaStream specified by |url|.
  void AddTracksToMediaStream(
      const std::string& url,
      const media::AudioParameters& params,
      scoped_refptr<media::AudioCapturerSource> audio,
      scoped_ptr<media::VideoCapturerSource> video);

  // |function| is a javascript function that will take |error_message| as
  // an argument. Called when something goes wrong in a cast receiver.
  void CallReceiverErrorCallback(
      v8::CopyablePersistentTraits<v8::Function>::CopyablePersistent function,
      const std::string& error_message);

  void CallGetRawEventsCallback(int transport_id,
                                scoped_ptr<base::BinaryValue> raw_events);
  void CallGetStatsCallback(int transport_id,
                            scoped_ptr<base::DictionaryValue> stats);

  // Gets the RTP stream or UDP transport indexed by an ID.
  // If not found, returns NULL and throws a V8 exception.
  CastRtpStream* GetRtpStreamOrThrow(int stream_id) const;
  CastUdpTransport* GetUdpTransportOrThrow(int transport_id) const;

  // Fills out a media::cast::FrameReceiverConfig from the v8
  // equivialent. (cast.streaming.receiverSession.RtpReceiverParams)
  // Returns true if everything was ok, raises a v8 exception and
  // returns false if anything went wrong.
  bool FrameReceiverConfigFromArg(
      v8::Isolate* isolate,
      const v8::Handle<v8::Value>& arg,
      media::cast::FrameReceiverConfig* config);

  bool IPEndPointFromArg(v8::Isolate* isolate,
                         const v8::Handle<v8::Value>& arg,
                         net::IPEndPoint* ip_endpoint);

  int last_transport_id_;

  typedef std::map<int, linked_ptr<CastRtpStream> > RtpStreamMap;
  RtpStreamMap rtp_stream_map_;

  typedef std::map<int, linked_ptr<CastUdpTransport> > UdpTransportMap;
  UdpTransportMap udp_transport_map_;

  extensions::ScopedPersistent<v8::Function> create_callback_;

  typedef std::map<int,
                   linked_ptr<extensions::ScopedPersistent<v8::Function> > >
      RtpStreamCallbackMap;
  RtpStreamCallbackMap get_raw_events_callbacks_;
  RtpStreamCallbackMap get_stats_callbacks_;

  base::WeakPtrFactory<CastStreamingNativeHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastStreamingNativeHandler);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CAST_STREAMING_NATIVE_HANDLER_H_
