// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "content/renderer/media/rtc_video_renderer.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/media/media_stream_audio_renderer.h"

namespace content {
namespace {

std::string GetMandatoryStreamConstraint(
    const WebKit::WebMediaConstraints& constraints, const std::string& key) {
  if (constraints.isNull())
    return std::string();

  WebKit::WebString value;
  constraints.getMandatoryConstraintValue(UTF8ToUTF16(key), value);
  return UTF16ToUTF8(value);
}

void UpdateRequestOptions(
    const WebKit::WebUserMediaRequest& user_media_request,
    StreamOptions* options) {
  if (options->audio_type != content::MEDIA_NO_SERVICE) {
    std::string audio_stream_source = GetMandatoryStreamConstraint(
        user_media_request.audioConstraints(), kMediaStreamSource);
    if (audio_stream_source == kMediaStreamSourceTab) {
      options->audio_type = content::MEDIA_TAB_AUDIO_CAPTURE;
      options->audio_device_id = GetMandatoryStreamConstraint(
          user_media_request.audioConstraints(),
          kMediaStreamSourceId);
    }
  }

  if (options->video_type != content::MEDIA_NO_SERVICE) {
    std::string video_stream_source = GetMandatoryStreamConstraint(
        user_media_request.videoConstraints(), kMediaStreamSource);
    if (video_stream_source == kMediaStreamSourceTab) {
      options->video_type = content::MEDIA_TAB_VIDEO_CAPTURE;
      options->video_device_id = GetMandatoryStreamConstraint(
          user_media_request.videoConstraints(),
          kMediaStreamSourceId);
    } else if (video_stream_source == kMediaStreamSourceScreen) {
      options->video_type = content::MEDIA_SCREEN_VIDEO_CAPTURE;
    }
  }
}

static int g_next_request_id  = 0;

// Creates a WebKit representation of a stream sources based on
// |devices| from the MediaStreamDispatcher.
void CreateWebKitSourceVector(
    const std::string& label,
    const StreamDeviceInfoArray& devices,
    WebKit::WebMediaStreamSource::Type type,
    WebKit::WebVector<WebKit::WebMediaStreamSource>& webkit_sources) {
  CHECK_EQ(devices.size(), webkit_sources.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    const char* track_type =
        (type == WebKit::WebMediaStreamSource::TypeAudio) ? "a" : "v";
    std::string source_id = StringPrintf("%s%s%u", label.c_str(),
                                         track_type,
                                         static_cast<unsigned int>(i));
    webkit_sources[i].initialize(
          UTF8ToUTF16(source_id),
          type,
          UTF8ToUTF16(devices[i].device.name));
    webkit_sources[i].setExtraData(
        new content::MediaStreamSourceExtraData(devices[i]));
  }
}

webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStream& descriptor) {
  content::MediaStreamExtraData* extra_data =
      static_cast<content::MediaStreamExtraData*>(descriptor.extraData());
  if (!extra_data)
    return NULL;
  return extra_data->stream();
}

}  // namespace

MediaStreamImpl::MediaStreamImpl(
    RenderView* render_view,
    MediaStreamDispatcher* media_stream_dispatcher,
    VideoCaptureImplManager* vc_manager,
    MediaStreamDependencyFactory* dependency_factory)
    : RenderViewObserver(render_view),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher),
      vc_manager_(vc_manager) {
}

MediaStreamImpl::~MediaStreamImpl() {
}

void MediaStreamImpl::OnLocalMediaStreamStop(
    const std::string& label) {
  DVLOG(1) << "MediaStreamImpl::OnLocalMediaStreamStop(" << label << ")";

  UserMediaRequestInfo* user_media_request = FindUserMediaRequestInfo(label);
  if (user_media_request) {
    media_stream_dispatcher_->StopStream(label);
    DeleteUserMediaRequestInfo(user_media_request);
  } else {
    DVLOG(1) << "MediaStreamImpl::OnLocalMediaStreamStop: the stream has "
             << "already been stopped.";
  }
}

void MediaStreamImpl::requestUserMedia(
    const WebKit::WebUserMediaRequest& user_media_request,
    const WebKit::WebVector<WebKit::WebMediaStreamSource>& audio_sources,
    const WebKit::WebVector<WebKit::WebMediaStreamSource>& video_sources) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK(CalledOnValidThread());
  int request_id = g_next_request_id++;
  StreamOptions options(MEDIA_NO_SERVICE, MEDIA_NO_SERVICE);
  WebKit::WebFrame* frame = NULL;
  GURL security_origin;

  // |user_media_request| can't be mocked. So in order to test at all we check
  // if it isNull.
  if (user_media_request.isNull()) {
    // We are in a test.
    if (audio_sources.size() > 0)
      options.audio_type = MEDIA_DEVICE_AUDIO_CAPTURE;
    if (video_sources.size() > 0)
      options.video_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  } else {
    if (user_media_request.audio())
      options.audio_type = MEDIA_DEVICE_AUDIO_CAPTURE;
    if (user_media_request.video())
      options.video_type = MEDIA_DEVICE_VIDEO_CAPTURE;

    security_origin = GURL(user_media_request.securityOrigin().toString());
    // Get the WebFrame that requested a MediaStream.
    // The frame is needed to tell the MediaStreamDispatcher when a stream goes
    // out of scope.
    frame = user_media_request.ownerDocument().frame();
    DCHECK(frame);

    UpdateRequestOptions(user_media_request, &options);
  }

  DVLOG(1) << "MediaStreamImpl::requestUserMedia(" << request_id << ", [ "
           << "audio=" << (options.audio_type)
           << ", video=" << (options.video_type) << " ], "
           << security_origin.spec() << ")";

  user_media_requests_.push_back(
      new UserMediaRequestInfo(request_id, frame, user_media_request));

  media_stream_dispatcher_->GenerateStream(
      request_id,
      AsWeakPtr(),
      options,
      security_origin);
}

void MediaStreamImpl::cancelUserMediaRequest(
    const WebKit::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  UserMediaRequestInfo* request = FindUserMediaRequestInfo(user_media_request);
  if (request) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    DeleteUserMediaRequestInfo(request);
  }
}

WebKit::WebMediaStream MediaStreamImpl::GetMediaStream(
    const GURL& url) {
  return WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);
}

bool MediaStreamImpl::IsMediaStream(const GURL& url) {
  return CheckMediaStream(url);
}

// static
bool MediaStreamImpl::CheckMediaStream(const GURL& url) {
  WebKit::WebMediaStream descriptor(
      WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));

  if (descriptor.isNull() || !descriptor.extraData())
    return false;  // This is not a valid stream.

  webrtc::MediaStreamInterface* stream = GetNativeMediaStream(descriptor);
  return (stream &&
      (!stream->GetVideoTracks().empty() || !stream->GetAudioTracks().empty()));
}

scoped_refptr<webkit_media::VideoFrameProvider>
MediaStreamImpl::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) {
  DCHECK(CalledOnValidThread());
  WebKit::WebMediaStream descriptor(GetMediaStream(url));

  if (descriptor.isNull() || !descriptor.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetVideoFrameProvider stream:"
           << UTF16ToUTF8(descriptor.label());

  webrtc::MediaStreamInterface* stream = GetNativeMediaStream(descriptor);
  if (stream)
    return CreateVideoFrameProvider(stream, error_cb, repaint_cb);
  NOTREACHED();
  return NULL;
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::GetVideoDecoder(
    const GURL& url,
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  DCHECK(CalledOnValidThread());
  WebKit::WebMediaStream descriptor(GetMediaStream(url));

  if (descriptor.isNull() || !descriptor.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetVideoDecoder stream:"
           << UTF16ToUTF8(descriptor.label());

  webrtc::MediaStreamInterface* stream = GetNativeMediaStream(descriptor);
  if (stream)
    return CreateVideoDecoder(stream, message_loop);
  NOTREACHED();
  return NULL;
}

scoped_refptr<webkit_media::MediaStreamAudioRenderer>
MediaStreamImpl::GetAudioRenderer(const GURL& url) {
  DCHECK(CalledOnValidThread());
  WebKit::WebMediaStream descriptor(GetMediaStream(url));

  if (descriptor.isNull() || !descriptor.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetAudioRenderer stream:"
           << UTF16ToUTF8(descriptor.label());

  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(descriptor.extraData());

  if (extra_data->is_local()) {
    // Create the local audio renderer if the stream contains audio tracks.
    return CreateLocalAudioRenderer(extra_data->stream());
  }

  // This is a remote media stream.
  WebRtcAudioDeviceImpl* audio_device =
      dependency_factory_->GetWebRtcAudioDevice();

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (!renderer) {
    renderer = CreateRemoteAudioRenderer(extra_data->stream());

    if (renderer && !audio_device->SetRenderer(renderer))
      renderer = NULL;
  }
  return renderer;
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void MediaStreamImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnStreamGenerated stream:" << label;

  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    // We need to tell the dispatcher to stop the stream.
    media_stream_dispatcher_->StopStream(label);
    DVLOG(1) << "Request ID not found";
    return;
  }
  request_info->generated = true;

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

  WebKit::WebUserMediaRequest* request = &(request_info->request);
  WebKit::WebString webkit_label = UTF8ToUTF16(label);
  WebKit::WebMediaStream* description = &(request_info->descriptor);

  description->initialize(webkit_label, audio_source_vector,
                          video_source_vector);

  // WebUserMediaRequest don't have an implementation in unit tests.
  // Therefore we need to check for isNull here.
  WebKit::WebMediaConstraints audio_constraints = request->isNull() ?
      WebKit::WebMediaConstraints() : request->audioConstraints();
  WebKit::WebMediaConstraints video_constraints = request->isNull() ?
      WebKit::WebMediaConstraints() : request->videoConstraints();

  dependency_factory_->CreateNativeMediaSources(
      audio_constraints, video_constraints, description,
      base::Bind(&MediaStreamImpl::OnCreateNativeSourcesComplete, AsWeakPtr()));
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void MediaStreamImpl::OnStreamGenerationFailed(int request_id) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnStreamGenerationFailed("
           << request_id << ")";
  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    return;
  }
  CompleteGetUserMediaRequest(request_info->descriptor,
                              &request_info->request,
                              false);
  DeleteUserMediaRequestInfo(request_info);
}

// Callback from MediaStreamDependencyFactory when the sources in |description|
// have been generated.
void MediaStreamImpl::OnCreateNativeSourcesComplete(
    WebKit::WebMediaStream* description,
    bool request_succeeded) {
  DVLOG(1) << "MediaStreamImpl::OnCreateNativeSourcesComplete stream:"
           << UTF16ToUTF8(description->label());
  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(description);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDependencyFactory is creating the sources.
    DVLOG(1) << "Request ID not found";
    return;
  }

  // Create a native representation of the stream.
  if (request_succeeded) {
    dependency_factory_->CreateNativeLocalMediaStream(
        description,
        base::Bind(&MediaStreamImpl::OnLocalMediaStreamStop, AsWeakPtr()));
  }
  CompleteGetUserMediaRequest(request_info->descriptor, &request_info->request,
                              request_succeeded);
  if (!request_succeeded) {
    OnLocalMediaStreamStop(UTF16ToUTF8(description->label()));
  }
}

void MediaStreamImpl::OnDevicesEnumerated(
    int request_id,
    const StreamDeviceInfoArray& device_array) {
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
    const StreamDeviceInfo& video_device) {
  DVLOG(1) << "MediaStreamImpl::OnDeviceOpened("
           << request_id << ", " << label << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "MediaStreamImpl::VideoDeviceOpenFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::CompleteGetUserMediaRequest(
    const WebKit::WebMediaStream& stream,
    WebKit::WebUserMediaRequest* request_info,
    bool request_succeeded) {
  if (request_succeeded) {
    request_info->requestSucceeded(stream);
  } else {
    request_info->requestFailed();
  }
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(int request_id) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->request_id == request_id)
      return (*it);
  }
  return NULL;
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(
    const WebKit::WebUserMediaRequest& request) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->request == request)
      return (*it);
  }
  return NULL;
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(const std::string& label) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->generated && (*it)->descriptor.label() == UTF8ToUTF16(label))
      return (*it);
  }
  return NULL;
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(
    WebKit::WebMediaStream* descriptor) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if (&((*it)->descriptor) == descriptor)
      return  (*it);
  }
  return NULL;
}

void MediaStreamImpl::DeleteUserMediaRequestInfo(
    UserMediaRequestInfo* request) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it) == request) {
      user_media_requests_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void MediaStreamImpl::FrameWillClose(WebKit::WebFrame* frame) {
  // Loop through all UserMediaRequests and find the requests that belong to the
  // frame that is being closed.
  UserMediaRequests::iterator request_it = user_media_requests_.begin();

  while (request_it != user_media_requests_.end()) {
    if ((*request_it)->frame == frame) {
      DVLOG(1) << "MediaStreamImpl::FrameWillClose: "
               << "Cancel user media request " << (*request_it)->request_id;
      // If the request is generated, it means that the MediaStreamDispatcher
      // has generated a stream for us and we need to let the
      // MediaStreamDispatcher know that the stream is no longer wanted.
      // If not, we cancel the request and delete the request object.
      if ((*request_it)->generated) {
        media_stream_dispatcher_->StopStream(
            UTF16ToUTF8((*request_it)->descriptor.label()));
      } else {
        media_stream_dispatcher_->CancelGenerateStream(
            (*request_it)->request_id);
      }
      request_it = user_media_requests_.erase(request_it);
    } else {
      ++request_it;
    }
  }
}

scoped_refptr<webkit_media::VideoFrameProvider>
MediaStreamImpl::CreateVideoFrameProvider(
    webrtc::MediaStreamInterface* stream,
    const base::Closure& error_cb,
    const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) {
  if (stream->GetVideoTracks().empty())
    return NULL;

  DVLOG(1) << "MediaStreamImpl::CreateRemoteVideoFrameProvider label:"
           << stream->label();

  return new RTCVideoRenderer(
      stream->GetVideoTracks()[0],
      error_cb,
      repaint_cb);
}

scoped_refptr<media::VideoDecoder> MediaStreamImpl::CreateVideoDecoder(
    webrtc::MediaStreamInterface* stream,
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  if (stream->GetVideoTracks().empty())
    return NULL;


  DVLOG(1) << "MediaStreamImpl::CreateRemoteVideoDecoder label:"
           << stream->label();

  return new RTCVideoDecoder(
      message_loop,
      base::MessageLoopProxy::current(),
      stream->GetVideoTracks()[0].get());
}

scoped_refptr<WebRtcAudioRenderer> MediaStreamImpl::CreateRemoteAudioRenderer(
    webrtc::MediaStreamInterface* stream) {
  if (stream->GetAudioTracks().empty())
    return NULL;

  DVLOG(1) << "MediaStreamImpl::CreateRemoteAudioRenderer label:"
           << stream->label();

  return new WebRtcAudioRenderer(RenderViewObserver::routing_id());
}

scoped_refptr<WebRtcLocalAudioRenderer>
MediaStreamImpl::CreateLocalAudioRenderer(
    webrtc::MediaStreamInterface* stream) {
  if (stream->GetAudioTracks().empty())
    return NULL;

  DVLOG(1) << "MediaStreamImpl::CreateLocalAudioRenderer label:"
           << stream->label();

  scoped_refptr<WebRtcAudioCapturer> source =
      dependency_factory_->GetWebRtcAudioDevice()->capturer();
  if (!source) {
    return NULL;
  }

  // Create a new WebRtcLocalAudioRenderer instance and connect it to the
  // existing WebRtcAudioCapturer so that the renderer can use it as source.
  return new WebRtcLocalAudioRenderer(source, RenderViewObserver::routing_id());
}

MediaStreamSourceExtraData::MediaStreamSourceExtraData(
    const StreamDeviceInfo& device_info)
    : device_info_(device_info) {
}

MediaStreamSourceExtraData::MediaStreamSourceExtraData(
    media::AudioCapturerSource* source)
    : audio_source_(source)  {
}

MediaStreamSourceExtraData::~MediaStreamSourceExtraData() {}

MediaStreamExtraData::MediaStreamExtraData(
    webrtc::MediaStreamInterface* stream, bool is_local)
    : stream_(stream),
      is_local_(is_local) {
}

MediaStreamExtraData::~MediaStreamExtraData() {
}

void MediaStreamExtraData::SetLocalStreamStopCallback(
    const StreamStopCallback& stop_callback) {
  stream_stop_callback_ = stop_callback;
}

void MediaStreamExtraData::OnLocalStreamStop() {
  if (!stream_stop_callback_.is_null())
    stream_stop_callback_.Run(stream_->label());
}

}  // namespace content
