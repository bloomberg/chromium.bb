// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_thread.h"
#include "content/renderer/media/capture_video_decoder.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/peer_connection_handler_jsep.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/message_loop_factory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

namespace {
const int kVideoCaptureWidth = 640;
const int kVideoCaptureHeight = 480;
const int kVideoCaptureFramePerSecond = 30;

// Helper enum used for histogramming calls to WebRTC APIs from JavaScript.
enum JavaScriptAPIName {
  kWebkitGetUserMedia,
  kWebkitPeerConnection,
  kInvalidName
};
}  // namespace

// Helper method used to collect information about the number of times
// different WebRTC API:s are called from JavaScript.
// The histogram can be viewed at chrome://histograms/WebRTC.webkitApiCount.
static void UpdateWebRTCMethodCount(JavaScriptAPIName api_name) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCount", api_name, kInvalidName);
}

static int g_next_request_id  = 0;

// MediaStreamSourceExtraData contains data stored in the
// WebKit::WebMediaStreamSource extra data field.
class MediaStreamSourceExtraData
    : public WebKit::WebMediaStreamSource::ExtraData {
 public:
  explicit MediaStreamSourceExtraData(
      const media_stream::StreamDeviceInfo& device_info)
      : device_info_(device_info) {
  }
  // Return device information about the camera or microphone.
  const media_stream::StreamDeviceInfo& device_info() const {
    return device_info_;
  }

 private:
  media_stream::StreamDeviceInfo device_info_;
};

// Creates a WebKit representation of a stream sources based on
// |devices| from the MediaStreamDispatcher.
static void CreateWebKitSourceVector(
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& devices,
    WebKit::WebMediaStreamSource::Type type,
    WebKit::WebVector<WebKit::WebMediaStreamSource>& webkit_sources) {
  ASSERT(devices.size() == webkit_sources.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    std::string source_id = StringPrintf("%s%d%u", label.c_str(), type,
                                         static_cast<unsigned int>(i));
    webkit_sources[i].initialize(
          UTF8ToUTF16(source_id),
          type,
          UTF8ToUTF16(devices[i].name));
    webkit_sources[i].setExtraData(
        new MediaStreamSourceExtraData(devices[i]));
  }
}

MediaStreamImpl::MediaStreamImpl(
    content::RenderView* render_view,
    MediaStreamDispatcher* media_stream_dispatcher,
    content::P2PSocketDispatcher* p2p_socket_dispatcher,
    VideoCaptureImplManager* vc_manager,
    MediaStreamDependencyFactory* dependency_factory)
    : content::RenderViewObserver(render_view),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      network_manager_(NULL),
      vc_manager_(vc_manager),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread") {
}

MediaStreamImpl::~MediaStreamImpl() {
  CleanupPeerConnectionFactory();
}

WebKit::WebPeerConnection00Handler*
MediaStreamImpl::CreatePeerConnectionHandlerJsep(
    WebKit::WebPeerConnection00HandlerClient* client) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitPeerConnection00.
  UpdateWebRTCMethodCount(kWebkitPeerConnection);
  DCHECK(CalledOnValidThread());
  if (!EnsurePeerConnectionFactory())
    return NULL;

  PeerConnectionHandlerJsep* pc_handler = new PeerConnectionHandlerJsep(
      client,
      dependency_factory_.get());
  return pc_handler;
}

void MediaStreamImpl::StopLocalMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  DVLOG(1) << "MediaStreamImpl::StopLocalMediaStream";

  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  if (extra_data && extra_data->local_stream()) {
    media_stream_dispatcher_->StopStream(extra_data->local_stream()->label());
    local_media_streams_.erase(extra_data->local_stream()->label());
  } else {
    NOTREACHED();
  }
}

void MediaStreamImpl::CreateMediaStream(
    WebKit::WebFrame* frame,
    WebKit::WebMediaStreamDescriptor* stream) {
  DVLOG(1) << "MediaStreamImpl::CreateMediaStream";

  if (!CreateNativeLocalMediaStream(stream)) {
    DVLOG(1) << "Failed to create native stream in CreateMediaStream.";
    return;
  }
}

void MediaStreamImpl::requestUserMedia(
    const WebKit::WebUserMediaRequest& user_media_request,
    const WebKit::WebVector<WebKit::WebMediaStreamSource>& audio_sources,
    const WebKit::WebVector<WebKit::WebMediaStreamSource>& video_sources) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(kWebkitGetUserMedia);
  DCHECK(CalledOnValidThread());
  int request_id = g_next_request_id++;
  bool audio = false;
  bool video = false;
  WebKit::WebFrame* frame = NULL;
  GURL security_origin;

  // |user_media_request| can't be mocked. So in order to test at all we check
  // if it isNull.
  if (user_media_request.isNull()) {
    // We are in a test.
    audio = audio_sources.size() > 0;
    video = video_sources.size() > 0;
  } else {
    audio = user_media_request.audio();
    video = user_media_request.video();
    security_origin = GURL(user_media_request.securityOrigin().toString());
    // Get the WebFrame that requested a MediaStream.
    // The frame is needed to tell the MediaStreamDispatcher when a stream goes
    // out of scope.
    frame = WebKit::WebFrame::frameForCurrentContext();
    DCHECK(frame);
  }

  DVLOG(1) << "MediaStreamImpl::generateStream(" << request_id << ", [ "
           << (audio ? "audio" : "")
           << (user_media_request.video() ? " video" : "") << "], "
           << security_origin.spec() << ")";

  user_media_requests_[request_id] =
      UserMediaRequestInfo(frame, user_media_request);

  media_stream_dispatcher_->GenerateStream(
      request_id,
      AsWeakPtr(),
      media_stream::StreamOptions(audio, video),
      security_origin);
}

void MediaStreamImpl::cancelUserMediaRequest(
    const WebKit::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  MediaRequestMap::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if (it->second.request_ == user_media_request)
      break;
  }
  if (it != user_media_requests_.end()) {
    media_stream_dispatcher_->CancelGenerateStream(it->first);
    user_media_requests_.erase(it);
  }
}

WebKit::WebMediaStreamDescriptor MediaStreamImpl::GetMediaStream(
    const GURL& url) {
  return WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::GetVideoDecoder(
    const GURL& url,
    media::MessageLoopFactory* message_loop_factory) {
  DCHECK(CalledOnValidThread());
  WebKit::WebMediaStreamDescriptor descriptor(GetMediaStream(url));

  if (descriptor.isNull() || !descriptor.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetVideoDecoder stream:"
           << UTF16ToUTF8(descriptor.label());

  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(descriptor.extraData());
  if (extra_data->local_stream())
    return CreateLocalVideoDecoder(extra_data->local_stream(),
                                   message_loop_factory);
  if (extra_data->remote_stream())
    return CreateRemoteVideoDecoder(extra_data->remote_stream(),
                                    message_loop_factory);
  NOTREACHED();
  return NULL;
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated.
void MediaStreamImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& audio_array,
    const media_stream::StreamDeviceInfoArray& video_array) {
  DCHECK(CalledOnValidThread());

  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_source_vector(
      audio_array.size());
  CreateWebKitSourceVector(label, audio_array,
                           WebKit::WebMediaStreamSource::TypeAudio,
                           audio_source_vector);
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_source_vector(
      video_array.size());
  CreateWebKitSourceVector(label, video_array,
                           WebKit::WebMediaStreamSource::TypeVideo,
                           video_source_vector);

  MediaRequestMap::iterator it = user_media_requests_.find(request_id);
  if (it == user_media_requests_.end()) {
    DVLOG(1) << "Request ID not found";
    media_stream_dispatcher_->StopStream(label);
    return;
  }

  WebKit::WebString webkit_label = UTF8ToUTF16(label);
  WebKit::WebMediaStreamDescriptor description;
  description.initialize(webkit_label, audio_source_vector,
                         video_source_vector);

  if (!CreateNativeLocalMediaStream(&description)) {
    DVLOG(1) << "Failed to create native stream in OnStreamGenerated.";
    media_stream_dispatcher_->StopStream(label);
    it->second.request_.requestFailed();
    user_media_requests_.erase(it);
    return;
  }
  local_media_streams_[label] = it->second.frame_;
  CompleteGetUserMediaRequest(description, &it->second.request_);
  user_media_requests_.erase(it);
}

void MediaStreamImpl::CompleteGetUserMediaRequest(
      const WebKit::WebMediaStreamDescriptor& stream,
      WebKit::WebUserMediaRequest* request) {
  request->requestSucceeded(stream);
}

void MediaStreamImpl::OnStreamGenerationFailed(int request_id) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnStreamGenerationFailed("
           << request_id << ")";
  MediaRequestMap::iterator it = user_media_requests_.find(request_id);
  if (it == user_media_requests_.end()) {
    DVLOG(1) << "Request ID not found";
    return;
  }
  WebKit::WebUserMediaRequest user_media_request(it->second.request_);
  user_media_requests_.erase(it);

  user_media_request.requestFailed();
}

void MediaStreamImpl::OnVideoDeviceFailed(const std::string& label,
                                          int index) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnVideoDeviceFailed("
           << label << ", " << index << ")";
  // TODO(grunell): Implement. Currently not supported in WebKit.
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnAudioDeviceFailed(const std::string& label,
                                          int index) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnAudioDeviceFailed("
           << label << ", " << index << ")";
  // TODO(grunell): Implement. Currently not supported in WebKit.
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnDevicesEnumerated(
    int request_id,
    const media_stream::StreamDeviceInfoArray& device_array) {
  DVLOG(1) << "MediaStreamImpl::OnDevicesEnumerated("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnDevicesEnumerationFailed(int request_id) {
  DVLOG(1) << "MediaStreamImpl::OnDevicesEnumerationFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfo& video_device) {
  DVLOG(1) << "MediaStreamImpl::OnDeviceOpened("
           << request_id << ", " << label << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "MediaStreamImpl::VideoDeviceOpenFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::FrameWillClose(WebKit::WebFrame* frame) {
  MediaRequestMap::iterator request_it = user_media_requests_.begin();
  while (request_it != user_media_requests_.end()) {
    if (request_it->second.frame_ == frame) {
      DVLOG(1) << "MediaStreamImpl::FrameWillClose: "
               << "Cancel user media request " << request_it->first;
      cancelUserMediaRequest(request_it->second.request_);
      request_it = user_media_requests_.begin();
    } else {
      ++request_it;
    }
  }
  LocalNativeStreamMap::iterator it = local_media_streams_.begin();
  while (it != local_media_streams_.end()) {
    if (it->second == frame) {
      DVLOG(1) << "MediaStreamImpl::FrameWillClose: "
               << "Stopping stream " << it->first;
      media_stream_dispatcher_->StopStream(it->first);
      local_media_streams_.erase(it);
      it = local_media_streams_.begin();
    } else {
      ++it;
    }
  }
}

void MediaStreamImpl::InitializeWorkerThread(talk_base::Thread** thread,
                                             base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();
  event->Signal();
}

void MediaStreamImpl::CreateIpcNetworkManagerOnWorkerThread(
    base::WaitableEvent* event) {
  DCHECK_EQ(MessageLoop::current(), chrome_worker_thread_.message_loop());
  network_manager_ = new content::IpcNetworkManager(p2p_socket_dispatcher_);
  event->Signal();
}

void MediaStreamImpl::DeleteIpcNetworkManager() {
  DCHECK_EQ(MessageLoop::current(), chrome_worker_thread_.message_loop());
  delete network_manager_;
  network_manager_ = NULL;
}

bool MediaStreamImpl::EnsurePeerConnectionFactory() {
  DCHECK(CalledOnValidThread());
  if (!signaling_thread_) {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
    signaling_thread_ = jingle_glue::JingleThreadWrapper::current();
  }

  if (!worker_thread_) {
    if (!chrome_worker_thread_.IsRunning()) {
      if (!chrome_worker_thread_.Start()) {
        LOG(ERROR) << "Could not start worker thread";
        signaling_thread_ = NULL;
        return false;
      }
    }
    base::WaitableEvent event(true, false);
    chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &MediaStreamImpl::InitializeWorkerThread,
        base::Unretained(this),
        &worker_thread_,
        &event));
    event.Wait();
    DCHECK(worker_thread_);
  }

  if (!network_manager_) {
    base::WaitableEvent event(true, false);
    chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
          &MediaStreamImpl::CreateIpcNetworkManagerOnWorkerThread,
          base::Unretained(this),
          &event));
    event.Wait();
  }

  if (!socket_factory_.get()) {
    socket_factory_.reset(
        new content::IpcPacketSocketFactory(p2p_socket_dispatcher_));
  }

  if (!dependency_factory_->PeerConnectionFactoryCreated()) {
    if (!dependency_factory_->CreatePeerConnectionFactory(
            worker_thread_,
            signaling_thread_,
            p2p_socket_dispatcher_,
            network_manager_,
            socket_factory_.get())) {
      LOG(ERROR) << "Could not create PeerConnection factory";
      return false;
    }
  }

  return true;
}

void MediaStreamImpl::CleanupPeerConnectionFactory() {
  if (dependency_factory_.get())
    dependency_factory_->ReleasePeerConnectionFactory();
  if (network_manager_) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the worked thread.
    if (chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
          &MediaStreamImpl::DeleteIpcNetworkManager,
          base::Unretained(this)));
      // Stopping the thread will wait until all tasks have been
      // processed before returning. We wait for the above task to finish before
      // letting the the function continue to avoid any potential race issues.
      chrome_worker_thread_.Stop();
    } else {
      NOTREACHED() << "Worker thread not running.";
    }
  }
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::CreateLocalVideoDecoder(
    webrtc::MediaStreamInterface* stream,
    media::MessageLoopFactory* message_loop_factory) {
  if (!stream->video_tracks() || stream->video_tracks()->count() == 0)
    return NULL;

  int video_session_id =
      media_stream_dispatcher_->video_session_id(stream->label(), 0);
  media::VideoCaptureCapability capability;
  capability.width = kVideoCaptureWidth;
  capability.height = kVideoCaptureHeight;
  capability.frame_rate = kVideoCaptureFramePerSecond;
  capability.color = media::VideoCaptureCapability::kI420;
  capability.expected_capture_delay = 0;
  capability.interlaced = false;

  DVLOG(1) << "MediaStreamImpl::CreateLocalVideoDecoder video_session_id:"
           << video_session_id;

  return new CaptureVideoDecoder(
      message_loop_factory->GetMessageLoop(
          media::MessageLoopFactory::kDecoder),
      video_session_id,
      vc_manager_.get(),
      capability);
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::CreateRemoteVideoDecoder(
    webrtc::MediaStreamInterface* stream,
    media::MessageLoopFactory* message_loop_factory) {
  if (!stream->video_tracks() || stream->video_tracks()->count() == 0)
    return NULL;

  DVLOG(1) << "MediaStreamImpl::CreateRemoteVideoDecoder label:"
           << stream->label();

  return new RTCVideoDecoder(
      message_loop_factory->GetMessageLoop(media::MessageLoopFactory::kDecoder),
      base::MessageLoopProxy::current(),
      stream->video_tracks()->at(0));
}

bool MediaStreamImpl::CreateNativeLocalMediaStream(
    WebKit::WebMediaStreamDescriptor* description) {
  // Creating the peer connection factory can fail if for example the audio
  // (input or output) or video device cannot be opened. Handling such cases
  // better is a higher level design discussion which involves the media
  // manager, webrtc and libjingle. We cannot create any native
  // track objects however, so we'll just have to skip that. Furthermore,
  // creating a peer connection later on will fail if we don't have a factory.
  if (!EnsurePeerConnectionFactory())
    return false;

  std::string label = UTF16ToUTF8(description->label());
  LocalNativeStreamPtr native_stream =
      dependency_factory_->CreateLocalMediaStream(label);

  // Add audio tracks.
  WebKit::WebVector<WebKit::WebMediaStreamComponent> audio_components;
  description->audioSources(audio_components);
  for (size_t i = 0; i < audio_components.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = audio_components[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(perkj): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }
    // TODO(perkj): Refactor the creation of audio tracks to use a proper
    // interface for receiving audio input data. Currently NULL is passed since
    // the |audio_device| is the wrong class and is unused.
    talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
        dependency_factory_->CreateLocalAudioTrack(
            UTF16ToUTF8(source.id()), NULL));
    native_stream->AddTrack(audio_track);
    audio_track->set_enabled(audio_components[i].isEnabled());
    // TODO(xians): This set the source of all audio tracks to the same
    // microphone. Implement support for setting the source per audio track
    // instead.
    dependency_factory_->SetAudioDeviceSessionId(
        source_data->device_info().session_id);
  }

  // Add video tracks.
  WebKit::WebVector<WebKit::WebMediaStreamComponent> video_components;
  description->videoSources(video_components);
  for (size_t i = 0; i < video_components.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = video_components[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(perkj): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }
    talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
        dependency_factory_->CreateLocalVideoTrack(
            UTF16ToUTF8(source.id()), source_data->device_info().session_id));
    native_stream->AddTrack(video_track);
    video_track->set_enabled(video_components[i].isEnabled());
  }

  description->setExtraData(new MediaStreamExtraData(native_stream));

  return true;
}

MediaStreamExtraData::MediaStreamExtraData(
    webrtc::MediaStreamInterface* remote_stream)
    : remote_stream_(remote_stream) {
}
MediaStreamExtraData::MediaStreamExtraData(
    webrtc::LocalMediaStreamInterface* local_stream)
    : local_stream_(local_stream) {
}
MediaStreamExtraData::~MediaStreamExtraData() {
}
