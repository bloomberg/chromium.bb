// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler.h"

#include <stdlib.h>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_impl.h"
#include "third_party/libjingle/source/talk/base/thread.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"

PeerConnectionHandler::PeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client,
    MediaStreamImpl* msi,
    MediaStreamDependencyFactory* dependency_factory,
    talk_base::Thread* signaling_thread,
    cricket::HttpPortAllocator* port_allocator)
    : client_(client),
      media_stream_impl_(msi),
      dependency_factory_(dependency_factory),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      signaling_thread_(signaling_thread),
      port_allocator_(port_allocator),
      call_state_(NOT_STARTED) {
}

PeerConnectionHandler::~PeerConnectionHandler() {
  if (native_peer_connection_.get()) {
    native_peer_connection_->RegisterObserver(NULL);
    native_peer_connection_->Close();
  }
}

bool PeerConnectionHandler::SetVideoRenderer(
    const std::string& stream_label,
    cricket::VideoRenderer* renderer) {
  return native_peer_connection_->SetVideoRenderer(stream_label, renderer);
}

void PeerConnectionHandler::initialize(
    const WebKit::WebString& server_configuration,
    const WebKit::WebSecurityOrigin& security_origin) {
  // We support the following server configuration format:
  // "STUN <address>:<port>". We only support STUN at the moment.

  // Strip "STUN ".
  std::string strip_string = "STUN ";
  std::string config = UTF16ToUTF8(server_configuration);
  size_t pos = config.find(strip_string);
  if (pos != 0) {
    DVLOG(1) << "Invalid configuration string.";
    return;
  }
  config = config.substr(strip_string.length());
  // Parse out port.
  pos = config.find(':');
  if (pos == std::string::npos) {
    DVLOG(1) << "Invalid configuration string.";
    return;
  }
  int port = 0;
  bool success = base::StringToInt(config.substr(pos+1), &port);
  if (!success || (port == 0)) {
    DVLOG(1) << "Invalid configuration string.";
    return;
  }
  // Get address.
  std::string address = config.substr(0, pos);

  std::vector<talk_base::SocketAddress> stun_hosts;
  stun_hosts.push_back(talk_base::SocketAddress(address, port));
  port_allocator_->SetStunHosts(stun_hosts);

  native_peer_connection_.reset(dependency_factory_->CreatePeerConnection(
      signaling_thread_));
  native_peer_connection_->RegisterObserver(this);
}

void PeerConnectionHandler::produceInitialOffer(
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
        pending_add_streams) {
  // We currently don't support creating an initial offer without a stream.
  // Native PeerConnection will anyway create the initial offer when the first
  // (and only) stream is added, so it will be done when processPendingStreams
  // is called if not here.
  if (pending_add_streams.isEmpty()) {
    DVLOG(1) << "Can't produce initial offer with no stream.";
    return;
  }
  // TODO(grunell): Support several streams.
  DCHECK_EQ(pending_add_streams.size(), 1u);
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector;
  pending_add_streams[0].sources(source_vector);
  DCHECK_GT(source_vector.size(), 0u);
  std::string label = UTF16ToUTF8(source_vector[0].id());
  AddStream(label);
}

void PeerConnectionHandler::handleInitialOffer(const WebKit::WebString& sdp) {
  native_peer_connection_->SignalingMessage(UTF16ToUTF8(sdp));
}

void PeerConnectionHandler::processSDP(const WebKit::WebString& sdp) {
  native_peer_connection_->SignalingMessage(UTF16ToUTF8(sdp));
}

void PeerConnectionHandler::processPendingStreams(
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
        pending_add_streams,
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
        pending_remove_streams) {
  // TODO(grunell): Support several streams.
  if (!pending_add_streams.isEmpty()) {
    DCHECK_EQ(pending_add_streams.size(), 1u);
    AddStream(UTF16ToUTF8(pending_add_streams[0].label()));
  }
  // Currently we ignore remove stream, no support in native PeerConnection.
}

void PeerConnectionHandler::sendDataStreamMessage(
    const char* data,
    size_t length) {
  // TODO(grunell): Implement. Not supported in native PeerConnection.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::stop() {
  if (native_peer_connection_.get())
    native_peer_connection_->RegisterObserver(NULL);
  native_peer_connection_.reset();
  // The close function will delete us.
  media_stream_impl_->ClosePeerConnection();
}

void PeerConnectionHandler::OnSignalingMessage(const std::string& msg) {
  if (!message_loop_proxy_->BelongsToCurrentThread()) {
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PeerConnectionHandler::OnSignalingMessage,
        base::Unretained(this),
        msg));
    return;
  }
  client_->didGenerateSDP(UTF8ToUTF16(msg));
}

void PeerConnectionHandler::OnAddStream(const std::string& stream_id,
                                        bool video) {
  if (!video)
    return;

  if (!message_loop_proxy_->BelongsToCurrentThread()) {
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PeerConnectionHandler::OnAddStreamCallback,
        base::Unretained(this),
        stream_id));
  } else {
    OnAddStreamCallback(stream_id);
  }
}

void PeerConnectionHandler::OnRemoveStream(
    const std::string& stream_id,
    bool video) {
  if (!video)
    return;

  if (!message_loop_proxy_->BelongsToCurrentThread()) {
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PeerConnectionHandler::OnRemoveStreamCallback,
        base::Unretained(this),
        remote_label_));
  } else {
    OnRemoveStreamCallback(remote_label_);
  }
}

void PeerConnectionHandler::AddStream(const std::string label) {
  // TODO(grunell): Fix code in this function after a new native PeerConnection
  // version has been rolled out.
  if (call_state_ == NOT_STARTED) {
    // TODO(grunell): Add audio and/or video depending on what's enabled
    // in the stream.
    std::string audio_label = label;
    audio_label.append("-audio");
    native_peer_connection_->AddStream(audio_label, false);  // Audio
    native_peer_connection_->AddStream(label, true);  // Video
    call_state_ = INITIATING;
  }
  if (call_state_ == INITIATING || call_state_ == RECEIVING) {
    local_label_ = label;
    if (media_stream_impl_->SetVideoCaptureModule(label))
      native_peer_connection_->SetVideoCapture("");
    if (call_state_ == INITIATING)
      native_peer_connection_->Connect();
    else if (call_state_ == RECEIVING)
      call_state_ = SENDING_AND_RECEIVING;
  } else {
    DLOG(ERROR) << "Multiple streams not supported";
  }
}

void PeerConnectionHandler::OnAddStreamCallback(
    const std::string& stream_label) {
  // TODO(grunell): Fix code in this function after a new native PeerConnection
  // version has been rolled out.
  if (call_state_ == NOT_STARTED) {
    remote_label_ = stream_label;
    call_state_ = RECEIVING;
  } else if (call_state_ == INITIATING) {
    remote_label_ = local_label_;
    remote_label_ += "-remote";
    call_state_ = SENDING_AND_RECEIVING;
  }

  // TODO(grunell): Support several tracks.
  WebKit::WebVector<WebKit::WebMediaStreamSource>
      source_vector(static_cast<size_t>(2));
  source_vector[0].initialize(WebKit::WebString::fromUTF8(remote_label_),
                              WebKit::WebMediaStreamSource::TypeVideo,
                              WebKit::WebString::fromUTF8("RemoteVideo"));
  source_vector[1].initialize(WebKit::WebString::fromUTF8(remote_label_),
                              WebKit::WebMediaStreamSource::TypeAudio,
                              WebKit::WebString::fromUTF8("RemoteAudio"));
  WebKit::WebMediaStreamDescriptor descriptor;
  descriptor.initialize(UTF8ToUTF16(remote_label_), source_vector);
  client_->didAddRemoteStream(descriptor);
}

void PeerConnectionHandler::OnRemoveStreamCallback(
    const std::string& stream_label) {
  // TODO(grunell): Support several tracks.
  WebKit::WebVector<WebKit::WebMediaStreamSource>
      source_vector(static_cast<size_t>(2));
  source_vector[0].initialize(WebKit::WebString::fromUTF8(stream_label),
                              WebKit::WebMediaStreamSource::TypeVideo,
                              WebKit::WebString::fromUTF8("RemoteVideo"));
  source_vector[1].initialize(WebKit::WebString::fromUTF8(stream_label),
                              WebKit::WebMediaStreamSource::TypeAudio,
                              WebKit::WebString::fromUTF8("RemoteAudio"));
  WebKit::WebMediaStreamDescriptor descriptor;
  descriptor.initialize(UTF8ToUTF16(stream_label), source_vector);
  client_->didRemoveRemoteStream(descriptor);
}
