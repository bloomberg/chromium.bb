// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"

MediaStreamImpl::MediaStreamImpl(
    MediaStreamDispatcher* media_stream_dispatcher,
    content::P2PSocketDispatcher* p2p_socket_dispatcher,
    VideoCaptureImplManager* vc_manager,
    MediaStreamDependencyFactory* dependency_factory)
    : dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher),
      media_engine_(NULL),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      network_manager_(NULL),
      vc_manager_(vc_manager),
      peer_connection_handler_(NULL),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread"),
      vcm_created_(false) {
}

MediaStreamImpl::~MediaStreamImpl() {
  DCHECK(!peer_connection_handler_);
  if (dependency_factory_.get())
    dependency_factory_->DeletePeerConnectionFactory();
}

WebKit::WebPeerConnectionHandler* MediaStreamImpl::CreatePeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client) {
  return NULL;
}

void MediaStreamImpl::ClosePeerConnection() {
}

bool MediaStreamImpl::SetVideoCaptureModule(const std::string& label) {
  return false;
}

void MediaStreamImpl::requestUserMedia(
    const WebKit::WebUserMediaRequest& user_media_request,
    const WebKit::WebVector<WebKit::WebMediaStreamSource>&
        media_stream_source_vector) {
}

void MediaStreamImpl::cancelUserMediaRequest(
    const WebKit::WebUserMediaRequest& user_media_request) {
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::GetVideoDecoder(
    const GURL& url,
    media::MessageLoopFactory* message_loop_factory) {
  return NULL;
}

void MediaStreamImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& audio_array,
    const media_stream::StreamDeviceInfoArray& video_array) {
}

void MediaStreamImpl::OnStreamGenerationFailed(int request_id) {
}

void MediaStreamImpl::OnVideoDeviceFailed(const std::string& label,
                                          int index) {
}

void MediaStreamImpl::OnAudioDeviceFailed(const std::string& label,
                                          int index) {
}
