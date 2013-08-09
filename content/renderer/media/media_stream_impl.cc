// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/desktop_media_id.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/rtc_video_renderer.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"

namespace content {
namespace {

std::string GetStreamConstraint(
    const WebKit::WebMediaConstraints& constraints, const std::string& key,
    bool is_mandatory) {
  if (constraints.isNull())
    return std::string();

  WebKit::WebString value;
  if (is_mandatory) {
    constraints.getMandatoryConstraintValue(UTF8ToUTF16(key), value);
  } else {
    constraints.getOptionalConstraintValue(UTF8ToUTF16(key), value);
  }
  return UTF16ToUTF8(value);
}

void UpdateRequestOptions(
    const WebKit::WebUserMediaRequest& user_media_request,
    StreamOptions* options) {
  if (options->audio_type != content::MEDIA_NO_SERVICE) {
    std::string audio_stream_source = GetStreamConstraint(
        user_media_request.audioConstraints(), kMediaStreamSource, true);
    if (audio_stream_source == kMediaStreamSourceTab) {
      options->audio_type = content::MEDIA_TAB_AUDIO_CAPTURE;
      options->audio_device_id = GetStreamConstraint(
          user_media_request.audioConstraints(),
          kMediaStreamSourceId, true);
    } else if (audio_stream_source == kMediaStreamSourceSystem) {
      options->audio_type = content::MEDIA_SYSTEM_AUDIO_CAPTURE;
    }
  }

  if (options->video_type != content::MEDIA_NO_SERVICE) {
    std::string video_stream_source = GetStreamConstraint(
        user_media_request.videoConstraints(), kMediaStreamSource, true);
    if (video_stream_source == kMediaStreamSourceTab) {
      options->video_type = content::MEDIA_TAB_VIDEO_CAPTURE;
      options->video_device_id = GetStreamConstraint(
          user_media_request.videoConstraints(),
          kMediaStreamSourceId, true);
    } else if (video_stream_source == kMediaStreamSourceScreen) {
      options->video_type = content::MEDIA_DESKTOP_VIDEO_CAPTURE;
      options->video_device_id =
          DesktopMediaID(DesktopMediaID::TYPE_SCREEN, 0).ToString();
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
    std::string source_id = base::StringPrintf("%s%s%u", label.c_str(),
                                               track_type,
                                               static_cast<unsigned int>(i));
    webkit_sources[i].initialize(
          UTF8ToUTF16(source_id),
          type,
          UTF8ToUTF16(devices[i].device.name));
    webkit_sources[i].setExtraData(
        new content::MediaStreamSourceExtraData(devices[i], webkit_sources[i]));
    webkit_sources[i].setDeviceId(UTF8ToUTF16(
        base::IntToString(devices[i].session_id)));
  }
}

webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStream& web_stream) {
  content::MediaStreamExtraData* extra_data =
      static_cast<content::MediaStreamExtraData*>(web_stream.extraData());
  if (!extra_data)
    return NULL;
  return extra_data->stream().get();
}

}  // namespace

MediaStreamImpl::MediaStreamImpl(
    RenderView* render_view,
    MediaStreamDispatcher* media_stream_dispatcher,
    MediaStreamDependencyFactory* dependency_factory)
    : RenderViewObserver(render_view),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher) {
}

MediaStreamImpl::~MediaStreamImpl() {
}

void MediaStreamImpl::OnLocalMediaStreamStop(
    const std::string& label) {
  DVLOG(1) << "MediaStreamImpl::OnLocalMediaStreamStop(" << label << ")";

  UserMediaRequestInfo* user_media_request = FindUserMediaRequestInfo(label);
  if (user_media_request) {
    StopLocalAudioTrack(user_media_request->web_stream);
    media_stream_dispatcher_->StopStream(label);
    DeleteUserMediaRequestInfo(user_media_request);
  } else {
    DVLOG(1) << "MediaStreamImpl::OnLocalMediaStreamStop: the stream has "
             << "already been stopped.";
  }
}

void MediaStreamImpl::requestUserMedia(
    const WebKit::WebUserMediaRequest& user_media_request) {
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
    options.audio_type = MEDIA_DEVICE_AUDIO_CAPTURE;
    options.video_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  } else {
    if (user_media_request.audio()) {
      options.audio_type = MEDIA_DEVICE_AUDIO_CAPTURE;
      options.audio_device_id = GetStreamConstraint(
          user_media_request.audioConstraints(),
          kMediaStreamSourceInfoId, false);
    }
    if (user_media_request.video()) {
      options.video_type = MEDIA_DEVICE_VIDEO_CAPTURE;
      options.video_device_id = GetStreamConstraint(
          user_media_request.videoConstraints(),
          kMediaStreamSourceInfoId, false);
    }

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
  WebKit::WebMediaStream web_stream(
      WebKit::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));

  if (web_stream.isNull() || !web_stream.extraData())
    return false;  // This is not a valid stream.

  webrtc::MediaStreamInterface* stream = GetNativeMediaStream(web_stream);
  return (stream &&
      (!stream->GetVideoTracks().empty() || !stream->GetAudioTracks().empty()));
}

scoped_refptr<VideoFrameProvider>
MediaStreamImpl::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb) {
  DCHECK(CalledOnValidThread());
  WebKit::WebMediaStream web_stream(GetMediaStream(url));

  if (web_stream.isNull() || !web_stream.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetVideoFrameProvider stream:"
           << UTF16ToUTF8(web_stream.id());

  webrtc::MediaStreamInterface* stream = GetNativeMediaStream(web_stream);
  if (stream)
    return CreateVideoFrameProvider(stream, error_cb, repaint_cb);
  NOTREACHED();
  return NULL;
}

scoped_refptr<MediaStreamAudioRenderer>
MediaStreamImpl::GetAudioRenderer(const GURL& url) {
  DCHECK(CalledOnValidThread());
  WebKit::WebMediaStream web_stream(GetMediaStream(url));

  if (web_stream.isNull() || !web_stream.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetAudioRenderer stream:"
           << UTF16ToUTF8(web_stream.id());

  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(web_stream.extraData());

  if (extra_data->is_local()) {
    // Create the local audio renderer if the stream contains audio tracks.
    return CreateLocalAudioRenderer(extra_data->stream().get());
  }

  webrtc::MediaStreamInterface* stream = extra_data->stream().get();
  if (!stream || stream->GetAudioTracks().empty())
    return NULL;

  // This is a remote media stream.
  WebRtcAudioDeviceImpl* audio_device =
      dependency_factory_->GetWebRtcAudioDevice();

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (!renderer.get()) {
    renderer = CreateRemoteAudioRenderer(extra_data->stream().get());

    if (renderer.get() && !audio_device->SetAudioRenderer(renderer.get()))
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
  request_info->audio_sources.assign(audio_source_vector);
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_source_vector(
      video_array.size());
  CreateWebKitSourceVector(label, video_array,
                           WebKit::WebMediaStreamSource::TypeVideo,
                           video_source_vector);
  request_info->video_sources.assign(video_source_vector);

  WebKit::WebUserMediaRequest* request = &(request_info->request);
  WebKit::WebString webkit_id = UTF8ToUTF16(label);
  WebKit::WebMediaStream* web_stream = &(request_info->web_stream);

  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  for (size_t i = 0; i < audio_track_vector.size(); ++i) {
    audio_track_vector[i].initialize(audio_source_vector[i].id(),
                                     audio_source_vector[i]);
  }

  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  for (size_t i = 0; i < video_track_vector.size(); ++i) {
    video_track_vector[i].initialize(video_source_vector[i].id(),
                                     video_source_vector[i]);
  }

  web_stream->initialize(webkit_id, audio_track_vector,
                         video_track_vector);

  // WebUserMediaRequest don't have an implementation in unit tests.
  // Therefore we need to check for isNull here.
  WebKit::WebMediaConstraints audio_constraints = request->isNull() ?
      WebKit::WebMediaConstraints() : request->audioConstraints();
  WebKit::WebMediaConstraints video_constraints = request->isNull() ?
      WebKit::WebMediaConstraints() : request->videoConstraints();

  dependency_factory_->CreateNativeMediaSources(
      RenderViewObserver::routing_id(),
      audio_constraints, video_constraints, web_stream,
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
  CompleteGetUserMediaRequest(request_info->web_stream,
                              &request_info->request,
                              false);
  DeleteUserMediaRequestInfo(request_info);
}

// Callback from MediaStreamDispatcher.
// The user has requested to stop the media stream.
void MediaStreamImpl::OnStopGeneratedStream(const std::string& label) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnStopGeneratedStream(" << label << ")";

  UserMediaRequestInfo* user_media_request = FindUserMediaRequestInfo(label);
  if (user_media_request) {
    // No need to call media_stream_dispatcher_->StopStream() because the
    // request has come from the browser process.
    StopLocalAudioTrack(user_media_request->web_stream);
    DeleteUserMediaRequestInfo(user_media_request);
  } else {
    DVLOG(1) << "MediaStreamImpl::OnStopGeneratedStream: the stream has "
             << "already been stopped.";
  }
}

// Callback from MediaStreamDependencyFactory when the sources in |web_stream|
// have been generated.
void MediaStreamImpl::OnCreateNativeSourcesComplete(
    WebKit::WebMediaStream* web_stream,
    bool request_succeeded) {
  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(web_stream);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDependencyFactory is creating the sources.
    DVLOG(1) << "Request ID not found";
    return;
  }

  // Create a native representation of the stream.
  if (request_succeeded) {
    dependency_factory_->CreateNativeLocalMediaStream(
        web_stream,
        base::Bind(&MediaStreamImpl::OnLocalMediaStreamStop, AsWeakPtr()));
  }
  CompleteGetUserMediaRequest(request_info->web_stream, &request_info->request,
                              request_succeeded);
  if (!request_succeeded) {
    OnLocalMediaStreamStop(UTF16ToUTF8(web_stream->id()));
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
    if ((*it)->generated && (*it)->web_stream.id() == UTF8ToUTF16(label))
      return (*it);
  }
  return NULL;
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(
    WebKit::WebMediaStream* web_stream) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if (&((*it)->web_stream) == web_stream)
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

void MediaStreamImpl::FrameDetached(WebKit::WebFrame* frame) {
  // Do same thing as FrameWillClose.
  FrameWillClose(frame);
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
        // Stop the local audio track before closing the device in the browser.
        StopLocalAudioTrack((*request_it)->web_stream);

        media_stream_dispatcher_->StopStream(
            UTF16ToUTF8((*request_it)->web_stream.id()));
      } else {
        media_stream_dispatcher_->CancelGenerateStream(
            (*request_it)->request_id, AsWeakPtr());
      }
      request_it = user_media_requests_.erase(request_it);
    } else {
      ++request_it;
    }
  }
}

scoped_refptr<VideoFrameProvider>
MediaStreamImpl::CreateVideoFrameProvider(
    webrtc::MediaStreamInterface* stream,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb) {
  if (stream->GetVideoTracks().empty())
    return NULL;

  DVLOG(1) << "MediaStreamImpl::CreateRemoteVideoFrameProvider label:"
           << stream->label();

  return new RTCVideoRenderer(
      stream->GetVideoTracks()[0],
      error_cb,
      repaint_cb);
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

  webrtc::AudioTrackVector audio_tracks = stream->GetAudioTracks();
  DCHECK_EQ(audio_tracks.size(), 1u);
  webrtc::AudioTrackInterface* audio_track = audio_tracks[0];
  DVLOG(1) << "audio_track.kind   : " << audio_track->kind()
           << "audio_track.id     : " << audio_track->id()
           << "audio_track.enabled: " << audio_track->enabled();

  // Create a new WebRtcLocalAudioRenderer instance and connect it to the
  // existing WebRtcAudioCapturer so that the renderer can use it as source.
  return new WebRtcLocalAudioRenderer(
      static_cast<WebRtcLocalAudioTrack*>(audio_track),
      RenderViewObserver::routing_id());
}

void MediaStreamImpl::StopLocalAudioTrack(
    const WebKit::WebMediaStream& web_stream) {
  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      web_stream.extraData());
  if (extra_data && extra_data->is_local() && extra_data->stream().get() &&
      !extra_data->stream()->GetAudioTracks().empty()) {
    webrtc::AudioTrackVector audio_tracks =
        extra_data->stream()->GetAudioTracks();
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      WebRtcLocalAudioTrack* audio_track = static_cast<WebRtcLocalAudioTrack*>(
          audio_tracks[i].get());
      // Remove the WebRtcAudioDevice as the sink to the local audio track.
      audio_track->RemoveSink(dependency_factory_->GetWebRtcAudioDevice());
      // Stop the audio track. This will unhook the audio track from the
      // capturer and will shutdown the source of the capturer if it is the
      // last audio track connecting to the capturer.
      audio_track->Stop();
    }
  }
}

MediaStreamSourceExtraData::MediaStreamSourceExtraData(
    const StreamDeviceInfo& device_info,
    const WebKit::WebMediaStreamSource& webkit_source)
    : device_info_(device_info),
      webkit_source_(webkit_source) {
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

MediaStreamImpl::UserMediaRequestInfo::UserMediaRequestInfo()
    : request_id(0), generated(false), frame(NULL), request() {
}

MediaStreamImpl::UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    WebKit::WebFrame* frame,
    const WebKit::WebUserMediaRequest& request)
    : request_id(request_id), generated(false), frame(frame),
      request(request) {
}

MediaStreamImpl::UserMediaRequestInfo::~UserMediaRequestInfo() {
  // Release the extra data field of all sources created by
  // MediaStreamImpl for this request. This breaks the circular reference to
  // WebKit::MediaStreamSource.
  // TODO(tommyw): Remove this once WebKit::MediaStreamSource::Owner has been
  // implemented to fully avoid a circular dependency.
  for (size_t i = 0; i < audio_sources.size(); ++i) {
    audio_sources[i].setReadyState(
        WebKit::WebMediaStreamSource::ReadyStateEnded);
    audio_sources[i].setExtraData(NULL);
  }

  for (size_t i = 0; i < video_sources.size(); ++i) {
    video_sources[i].setReadyState(
            WebKit::WebMediaStreamSource::ReadyStateEnded);
    video_sources[i].setExtraData(NULL);
  }
}

}  // namespace content
