// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/capture_video_decoder.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/peer_connection_handler.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_module_impl.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/message_loop_factory.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "third_party/libjingle/source/talk/session/phone/dummydevicemanager.h"
#include "third_party/libjingle/source/talk/session/phone/webrtcmediaengine.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"

namespace {

static const int kVideoCaptureWidth = 352;
static const int kVideoCaptureHeight = 288;
static const int kVideoCaptureFramePerSecond = 30;

}  // namespace

int MediaStreamImpl::next_request_id_ = 0;

MediaStreamImpl::MediaStreamImpl(
    MediaStreamDispatcher* media_stream_dispatcher,
    content::P2PSocketDispatcher* p2p_socket_dispatcher,
    VideoCaptureImplManager* vc_manager,
    MediaStreamDependencyFactory* dependency_factory)
    : dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher),
      media_engine_(NULL),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      vc_manager_(vc_manager),
      port_allocator_(NULL),
      peer_connection_handler_(NULL),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread"),
      vcm_created_(false) {
}

MediaStreamImpl::~MediaStreamImpl() {
  if (dependency_factory_.get())
    dependency_factory_->DeletePeerConnectionFactory();
}

WebKit::WebPeerConnectionHandler* MediaStreamImpl::CreatePeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client) {
  if (peer_connection_handler_) {
    DVLOG(1) << "A PeerConnection already exists";
    return NULL;
  }

  if (!media_engine_) {
    media_engine_ = dependency_factory_->CreateWebRtcMediaEngine();
  }

  if (!signaling_thread_) {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentThread();
    jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
    signaling_thread_ = jingle_glue::JingleThreadWrapper::current();
  }

  if (!worker_thread_) {
    if (!chrome_worker_thread_.IsRunning()) {
      if (!chrome_worker_thread_.Start()) {
        LOG(ERROR) << "Could not start worker thread";
        delete media_engine_;
        media_engine_ = NULL;
        signaling_thread_ = NULL;
        return NULL;
      }
    }
    base::WaitableEvent event(true, false);
    chrome_worker_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MediaStreamImpl::InitializeWorkerThread, this,
                   &worker_thread_, &event));
    event.Wait();
    DCHECK(worker_thread_);
  }

  if (!dependency_factory_->PeerConnectionFactoryCreated()) {
    ipc_network_manager_.reset(
        new content::IpcNetworkManager(p2p_socket_dispatcher_));
    ipc_socket_factory_.reset(
        new content::IpcPacketSocketFactory(p2p_socket_dispatcher_));
    port_allocator_ = new cricket::HttpPortAllocator(
        ipc_network_manager_.get(),
        ipc_socket_factory_.get(),
        "PeerConnection");
    // TODO(mallinath): The following flags were added to solve a crash in
    // HttpClient, we should probably remove them after that issue has been
    // investigated.
    port_allocator_->set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                              cricket::PORTALLOCATOR_DISABLE_RELAY);

    if (!dependency_factory_->CreatePeerConnectionFactory(port_allocator_,
                                                          media_engine_,
                                                          worker_thread_)) {
      LOG(ERROR) << "Could not initialize PeerConnection factory";
      return NULL;
    }
  }

  peer_connection_handler_ = new PeerConnectionHandler(
      client,
      this,
      dependency_factory_.get(),
      signaling_thread_,
      port_allocator_);

  return peer_connection_handler_;
}

void MediaStreamImpl::ClosePeerConnection() {
  rtc_video_decoder_ = NULL;
  media_engine_->SetVideoCaptureModule(NULL);
  vcm_created_ = false;
  peer_connection_handler_ = NULL;
}

bool MediaStreamImpl::SetVideoCaptureModule(const std::string& label) {
  if (vcm_created_)
    return true;
  // Set the capture device.
  // TODO(grunell): Instead of using the first track, the selected track
  // should be used.
  int id = media_stream_dispatcher_->video_session_id(label, 0);
  if (id == media_stream::StreamDeviceInfo::kNoId)
    return false;
  webrtc::VideoCaptureModule* vcm =
      new VideoCaptureModuleImpl(id, vc_manager_.get());
  vcm_created_ = true;
  media_engine_->SetVideoCaptureModule(vcm);
  return true;
}

void MediaStreamImpl::requestUserMedia(
    const WebKit::WebUserMediaRequest& user_media_request,
    const WebKit::WebVector<WebKit::WebMediaStreamSource>&
        media_stream_source_vector) {
  DCHECK(!user_media_request.isNull());

  int request_id = next_request_id_++;

  bool audio = user_media_request.audio();
  media_stream::StreamOptions::VideoOption video_option =
      media_stream::StreamOptions::kNoCamera;
  if (user_media_request.video()) {
    // If no preference is set, use user facing camera.
    video_option = media_stream::StreamOptions::kFacingUser;
    if (user_media_request.cameraPreferenceUser() &&
        user_media_request.cameraPreferenceEnvironment()) {
      video_option = media_stream::StreamOptions::kFacingBoth;
    } else if (user_media_request.cameraPreferenceEnvironment()) {
      video_option = media_stream::StreamOptions::kFacingEnvironment;
    }
  }

  std::string security_origin = UTF16ToUTF8(
      user_media_request.securityOrigin().toString());

  DVLOG(1) << "MediaStreamImpl::generateStream(" << request_id << ", [ "
           << (audio ? "audio " : "")
           << ((user_media_request.cameraPreferenceUser()) ?
               "video_facing_user " : "")
           << ((user_media_request.cameraPreferenceEnvironment()) ?
               "video_facing_environment " : "") << "], "
           << security_origin << ")";

  user_media_requests_.insert(
      std::pair<int, WebKit::WebUserMediaRequest>(
          request_id, user_media_request));

  media_stream_dispatcher_->GenerateStream(
      request_id,
      this,
      media_stream::StreamOptions(audio, video_option),
      security_origin);
}

void MediaStreamImpl::cancelUserMediaRequest(
    const WebKit::WebUserMediaRequest& user_media_request) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::GetVideoDecoder(
    const GURL& url,
    media::MessageLoopFactory* message_loop_factory) {
  WebKit::WebMediaStreamDescriptor descriptor(
      WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));
  if (descriptor.isNull())
    return NULL;  // This is not a valid stream.
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector;
  descriptor.sources(source_vector);
  std::string label;
  for (size_t i = 0; i < source_vector.size(); ++i) {
    if (source_vector[i].type() == WebKit::WebMediaStreamSource::TypeVideo) {
      label = UTF16ToUTF8(source_vector[i].id());
      break;
    }
  }
  if (label.empty())
    return NULL;

  scoped_refptr<media::VideoDecoder> decoder;
  if (media_stream_dispatcher_->IsStream(label)) {
    // It's a local stream.
    int video_session_id = media_stream_dispatcher_->video_session_id(label, 0);
    media::VideoCapture::VideoCaptureCapability capability;
    capability.width = kVideoCaptureWidth;
    capability.height = kVideoCaptureHeight;
    capability.max_fps = kVideoCaptureFramePerSecond;
    capability.expected_capture_delay = 0;
    capability.raw_type = media::VideoFrame::I420;
    capability.interlaced = false;
    decoder = new CaptureVideoDecoder(
        message_loop_factory->GetMessageLoopProxy("CaptureVideoDecoderThread"),
        video_session_id,
        vc_manager_.get(),
        capability);
  } else {
    // It's a remote stream.
    size_t found = label.rfind("-remote");
    if (found != std::string::npos)
      label = label.substr(0, found);
    if (rtc_video_decoder_.get()) {
      // The renderer is used by PeerConnection, release it first.
      if (peer_connection_handler_)
        peer_connection_handler_->SetVideoRenderer(label, NULL);
    }
    rtc_video_decoder_ = new RTCVideoDecoder(
        message_loop_factory->GetMessageLoop("RtcVideoDecoderThread"),
        url.spec());
    decoder = rtc_video_decoder_;
    if (peer_connection_handler_)
      peer_connection_handler_->SetVideoRenderer(label, rtc_video_decoder_);
  }
  return decoder;
}

void MediaStreamImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& audio_array,
    const media_stream::StreamDeviceInfoArray& video_array) {
  // We only support max one audio track and one video track. If the UI
  // for selecting device starts to allow several devices, we must implement
  // handling for this.
  DCHECK_LE(audio_array.size(), 1u);
  DCHECK_LE(video_array.size(), 1u);
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector(
      audio_array.size() + video_array.size());

  WebKit::WebString track_label_audio(UTF8ToUTF16("AudioDevice"));
  WebKit::WebString track_label_video(UTF8ToUTF16("VideoCapture"));
  size_t track_num = source_vector.size();
  while (track_num--) {
    if (track_num < audio_array.size()) {
      source_vector[track_num].initialize(
          UTF8ToUTF16(label),
          WebKit::WebMediaStreamSource::TypeAudio,
          track_label_audio);
    } else {
      source_vector[track_num].initialize(
          UTF8ToUTF16(label),
          WebKit::WebMediaStreamSource::TypeVideo,
          track_label_video);
    }
  }

  MediaRequestMap::iterator it = user_media_requests_.find(request_id);
  if (it == user_media_requests_.end()) {
    DVLOG(1) << "Request ID not found";
    return;
  }
  WebKit::WebUserMediaRequest user_media_request = it->second;
  user_media_requests_.erase(it);
  stream_labels_.push_back(label);

  user_media_request.requestSucceeded(source_vector);
}

void MediaStreamImpl::OnStreamGenerationFailed(int request_id) {
  DVLOG(1) << "MediaStreamImpl::OnStreamGenerationFailed("
           << request_id << ")";
  MediaRequestMap::iterator it = user_media_requests_.find(request_id);
  if (it == user_media_requests_.end()) {
    DVLOG(1) << "Request ID not found";
    return;
  }
  WebKit::WebUserMediaRequest user_media_request = it->second;
  user_media_requests_.erase(it);

  user_media_request.requestFailed();
}

void MediaStreamImpl::OnVideoDeviceFailed(const std::string& label,
                                          int index) {
  DVLOG(1) << "MediaStreamImpl::OnVideoDeviceFailed("
           << label << ", " << index << ")";
  // TODO(grunell): Implement. Currently not supported in WebKit.
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnAudioDeviceFailed(const std::string& label,
                                          int index) {
  DVLOG(1) << "MediaStreamImpl::OnAudioDeviceFailed("
           << label << ", " << index << ")";
  // TODO(grunell): Implement. Currently not supported in WebKit.
  NOTIMPLEMENTED();
}

void MediaStreamImpl::InitializeWorkerThread(talk_base::Thread** thread,
                                             base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentThread();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();
  event->Signal();
}
