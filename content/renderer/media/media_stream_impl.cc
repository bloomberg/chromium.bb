// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
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
      options->audio_type = content::MEDIA_LOOPBACK_AUDIO_CAPTURE;
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
    } else if (video_stream_source == kMediaStreamSourceDesktop) {
      options->video_type = content::MEDIA_DESKTOP_VIDEO_CAPTURE;
      options->video_device_id = GetStreamConstraint(
          user_media_request.videoConstraints(),
          kMediaStreamSourceId, true);
    }
  }
}

static int g_next_request_id  = 0;

webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStream& web_stream) {
  content::MediaStreamExtraData* extra_data =
      static_cast<content::MediaStreamExtraData*>(web_stream.extraData());
  if (!extra_data)
    return NULL;
  return extra_data->stream().get();
}

void GetDefaultOutputDeviceParams(
    int* output_sample_rate, int* output_buffer_size) {
  // Fetch the default audio output hardware config.
  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();
  *output_sample_rate = hardware_config->GetOutputSampleRate();
  *output_buffer_size = hardware_config->GetOutputBufferSize();
}

void RemoveSource(const WebKit::WebMediaStreamSource& source,
                  std::vector<WebKit::WebMediaStreamSource>* sources) {
  for (std::vector<WebKit::WebMediaStreamSource>::iterator it =
           sources->begin();
       it != sources->end(); ++it) {
    if (source.id() == it->id()) {
      sources->erase(it);
      return;
    }
  }
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
  bool enable_automatic_output_device_selection = false;

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
      // Check if this input device should be used to select a matching output
      // device for audio rendering.
      std::string enable = GetStreamConstraint(
          user_media_request.audioConstraints(),
          kMediaStreamRenderToAssociatedSink, false);
      if (LowerCaseEqualsASCII(enable, "true"))
        enable_automatic_output_device_selection = true;
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
           << " select associated sink: "
           << enable_automatic_output_device_selection
           << ", video=" << (options.video_type) << " ], "
           << security_origin.spec() << ")";

  user_media_requests_.push_back(
      new UserMediaRequestInfo(request_id, frame, user_media_request,
          enable_automatic_output_device_selection));

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

  return renderer.get() ? renderer->CreateSharedAudioRendererProxy() : NULL;
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
    // Only stop the device if the device is not used in another MediaStream.
    for (StreamDeviceInfoArray::const_iterator device_it = audio_array.begin();
         device_it != audio_array.end(); ++device_it) {
      if (!FindLocalSource(*device_it))
        media_stream_dispatcher_->StopStreamDevice(*device_it);
    }

    for (StreamDeviceInfoArray::const_iterator device_it = video_array.begin();
         device_it != video_array.end(); ++device_it) {
      if (!FindLocalSource(*device_it))
        media_stream_dispatcher_->StopStreamDevice(*device_it);
    }

    DVLOG(1) << "Request ID not found";
    return;
  }
  request_info->generated = true;

  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_source_vector(
        audio_array.size());

  StreamDeviceInfoArray overridden_audio_array = audio_array;
  if (!request_info->enable_automatic_output_device_selection) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (StreamDeviceInfoArray::iterator it = overridden_audio_array.begin();
         it != overridden_audio_array.end(); ++it) {
      it->device.matched_output_device_id = "";
      it->device.matched_output = MediaStreamDevice::AudioDeviceParameters();
    }
  }
  CreateWebKitSourceVector(label, overridden_audio_array,
                           WebKit::WebMediaStreamSource::TypeAudio,
                           request_info->frame,
                           audio_source_vector);

  WebKit::WebVector<WebKit::WebMediaStreamSource> video_source_vector(
      video_array.size());
  CreateWebKitSourceVector(label, video_array,
                           WebKit::WebMediaStreamSource::TypeVideo,
                           request_info->frame,
                           video_source_vector);
  WebKit::WebUserMediaRequest* request = &(request_info->request);
  WebKit::WebString webkit_id = UTF8ToUTF16(label);
  WebKit::WebMediaStream* web_stream = &(request_info->web_stream);

  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  for (size_t i = 0; i < audio_track_vector.size(); ++i) {
    audio_track_vector[i].initialize(audio_source_vector[i]);
    request_info->sources.push_back(audio_source_vector[i]);
  }

  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  for (size_t i = 0; i < video_track_vector.size(); ++i) {
    video_track_vector[i].initialize(video_source_vector[i]);
    request_info->sources.push_back(video_source_vector[i]);
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
    DeleteUserMediaRequestInfo(user_media_request);
  }
  StopUnreferencedSources(false);
}

void MediaStreamImpl::CreateWebKitSourceVector(
    const std::string& label,
    const StreamDeviceInfoArray& devices,
    WebKit::WebMediaStreamSource::Type type,
    WebKit::WebFrame* frame,
    WebKit::WebVector<WebKit::WebMediaStreamSource>& webkit_sources) {
  CHECK_EQ(devices.size(), webkit_sources.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    const char* track_type =
        (type == WebKit::WebMediaStreamSource::TypeAudio) ? "a" : "v";
    std::string source_id = base::StringPrintf("%s%s%u", label.c_str(),
                                               track_type,
                                               static_cast<unsigned int>(i));

    const WebKit::WebMediaStreamSource* existing_source =
        FindLocalSource(devices[i]);
    if (existing_source) {
      webkit_sources[i] = *existing_source;
      DVLOG(1) << "Source already exist. Reusing source with id "
               << webkit_sources[i]. id().utf8();
      continue;
    }
    webkit_sources[i].initialize(
        UTF8ToUTF16(source_id),
        type,
        UTF8ToUTF16(devices[i].device.name));
    MediaStreamSourceExtraData* source_extra_data(
        new content::MediaStreamSourceExtraData(
            devices[i],
            base::Bind(&MediaStreamImpl::OnLocalSourceStop, AsWeakPtr())));
    // |source_extra_data| is owned by webkit_sources[i].
    webkit_sources[i].setExtraData(source_extra_data);
    webkit_sources[i].setDeviceId(UTF8ToUTF16(
        base::IntToString(devices[i].session_id)));
    local_sources_.push_back(LocalStreamSource(frame, webkit_sources[i]));
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
  DVLOG(1) << "MediaStreamImpl::OnCreateNativeSourcesComplete("
           << "{request_id = " << request_info->request_id << "} "
           << "{request_succeeded = " << request_succeeded << "})";
  CompleteGetUserMediaRequest(request_info->web_stream, &request_info->request,
                              request_succeeded);
  if (!request_succeeded) {
    // TODO(perkj): Once we don't support MediaStream::Stop the |request_info|
    // can be deleted even if the request succeeds.
    DeleteUserMediaRequestInfo(request_info);
    StopUnreferencedSources(true);
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

const WebKit::WebMediaStreamSource* MediaStreamImpl::FindLocalSource(
    const StreamDeviceInfo& device) const {
  for (LocalStreamSources::const_iterator it = local_sources_.begin();
       it != local_sources_.end(); ++it) {
    MediaStreamSourceExtraData* extra_data =
        static_cast<MediaStreamSourceExtraData*>(
            it->source.extraData());
    const StreamDeviceInfo& active_device = extra_data->device_info();
    if (active_device.device.id == device.device.id &&
        active_device.session_id == device.session_id) {
      return &it->source;
    }
  }
  return NULL;
}

bool MediaStreamImpl::FindSourceInRequests(
    const WebKit::WebMediaStreamSource& source) const {
  for (UserMediaRequests::const_iterator req_it = user_media_requests_.begin();
       req_it != user_media_requests_.end(); ++req_it) {
    const std::vector<WebKit::WebMediaStreamSource>& sources =
        (*req_it)->sources;
    for (std::vector<WebKit::WebMediaStreamSource>::const_iterator source_it =
             sources.begin();
         source_it != sources.end(); ++source_it) {
      if (source_it->id() == source.id()) {
        return true;
      }
    }
  }
  return false;
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
      // If the request is not generated, it means that a request
      // has been sent to the MediaStreamDispatcher to generate a stream
      // but MediaStreamDispatcher has not yet responded and we need to cancel
      // the request.
      if (!(*request_it)->generated) {
        media_stream_dispatcher_->CancelGenerateStream(
            (*request_it)->request_id, AsWeakPtr());
      }
      request_it = user_media_requests_.erase(request_it);
    } else {
      ++request_it;
    }
  }

  // Loop through all current local sources and stop the sources that were
  // created by the frame that will be closed.
  LocalStreamSources::iterator sources_it = local_sources_.begin();
  while (sources_it != local_sources_.end()) {
    if (sources_it->frame == frame) {
      StopLocalSource(sources_it->source, true);
      sources_it = local_sources_.erase(sources_it);
    } else {
      ++sources_it;
    }
  }
}

void MediaStreamImpl::OnLocalMediaStreamStop(
    const std::string& label) {
  DVLOG(1) << "MediaStreamImpl::OnLocalMediaStreamStop(" << label << ")";

  UserMediaRequestInfo* user_media_request = FindUserMediaRequestInfo(label);
  if (user_media_request) {
    DeleteUserMediaRequestInfo(user_media_request);
  }
  StopUnreferencedSources(true);
}

void MediaStreamImpl::OnLocalSourceStop(
    const WebKit::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());

  StopLocalSource(source, true);

  bool device_found = false;
  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (device_it->source.id()  == source.id()) {
      device_found = true;
      local_sources_.erase(device_it);
      break;
    }
  }
  CHECK(device_found);

  // Remove the reference to this source from all |user_media_requests_|.
  // TODO(perkj): The below is not necessary once we don't need to support
  // MediaStream::Stop().
  UserMediaRequests::iterator it = user_media_requests_.begin();
  while (it != user_media_requests_.end()) {
    RemoveSource(source, &(*it)->sources);
    if ((*it)->sources.empty()) {
      it = user_media_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

void MediaStreamImpl::StopLocalSource(
    const WebKit::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSourceExtraData* extra_data =
        static_cast<MediaStreamSourceExtraData*> (source.extraData());
  CHECK(extra_data);
  DVLOG(1) << "MediaStreamImpl::StopLocalSource("
           << "{device_id = " << extra_data->device_info().device.id << "})";

  if (source.type() == WebKit::WebMediaStreamSource::TypeAudio) {
    if (extra_data->GetAudioCapturer()) {
      extra_data->GetAudioCapturer()->Stop();
    }
  }

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(extra_data->device_info());

  WebKit::WebMediaStreamSource writable_source(source);
  writable_source.setReadyState(
      WebKit::WebMediaStreamSource::ReadyStateEnded);
  writable_source.setExtraData(NULL);
}

void MediaStreamImpl::StopUnreferencedSources(bool notify_dispatcher) {
  LocalStreamSources::iterator source_it = local_sources_.begin();
  while (source_it != local_sources_.end()) {
    if (!FindSourceInRequests(source_it->source)) {
      StopLocalSource(source_it->source, notify_dispatcher);
      source_it = local_sources_.erase(source_it);
    } else {
      ++source_it;
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

  // TODO(tommi): Change the default value of session_id to be
  // StreamDeviceInfo::kNoId.  Also update AudioOutputDevice etc.
  int session_id = 0, sample_rate = 0, buffer_size = 0;
  if (!GetAuthorizedDeviceInfoForAudioRenderer(&session_id,
                                               &sample_rate,
                                               &buffer_size)) {
    GetDefaultOutputDeviceParams(&sample_rate, &buffer_size);
  }

  return new WebRtcAudioRenderer(RenderViewObserver::routing_id(),
      session_id, sample_rate, buffer_size);
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

  int session_id = 0, sample_rate = 0, buffer_size = 0;
  if (!GetAuthorizedDeviceInfoForAudioRenderer(&session_id,
                                               &sample_rate,
                                               &buffer_size)) {
    GetDefaultOutputDeviceParams(&sample_rate, &buffer_size);
  }

  // Create a new WebRtcLocalAudioRenderer instance and connect it to the
  // existing WebRtcAudioCapturer so that the renderer can use it as source.
  return new WebRtcLocalAudioRenderer(
      static_cast<WebRtcLocalAudioTrack*>(audio_track),
      RenderViewObserver::routing_id(),
      session_id,
      sample_rate,
      buffer_size);
}

bool MediaStreamImpl::GetAuthorizedDeviceInfoForAudioRenderer(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) {
  DCHECK(CalledOnValidThread());

  WebRtcAudioDeviceImpl* audio_device =
      dependency_factory_->GetWebRtcAudioDevice();
  if (!audio_device)
    return false;

  if (!audio_device->GetDefaultCapturer())
    return false;

  return audio_device->GetDefaultCapturer()->GetPairedOutputParameters(
      session_id,
      output_sample_rate,
      output_frames_per_buffer);
}

MediaStreamSourceExtraData::MediaStreamSourceExtraData(
    const StreamDeviceInfo& device_info,
    const SourceStopCallback& stop_callback)
    : device_info_(device_info),
      stop_callback_(stop_callback) {
}

MediaStreamSourceExtraData::MediaStreamSourceExtraData() {
}

MediaStreamSourceExtraData::~MediaStreamSourceExtraData() {}

void MediaStreamSourceExtraData::OnLocalSourceStop() {
  if (!stop_callback_.is_null())
    stop_callback_.Run(owner());
}

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

MediaStreamImpl::UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    WebKit::WebFrame* frame,
    const WebKit::WebUserMediaRequest& request,
    bool enable_automatic_output_device_selection)
    : request_id(request_id),
      generated(false),
      enable_automatic_output_device_selection(
          enable_automatic_output_device_selection),
      frame(frame),
      request(request) {
}

MediaStreamImpl::UserMediaRequestInfo::~UserMediaRequestInfo() {
}

}  // namespace content
