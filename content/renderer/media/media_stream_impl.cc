// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/rtc_video_renderer.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "content/renderer/media/webrtc_logging.h"
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

void CopyStreamConstraints(const blink::WebMediaConstraints& constraints,
                           StreamOptions::Constraints* mandatory,
                           StreamOptions::Constraints* optional) {
  blink::WebVector<blink::WebMediaConstraint> mandatory_constraints;
  constraints.getMandatoryConstraints(mandatory_constraints);
  for (size_t i = 0; i < mandatory_constraints.size(); i++) {
    mandatory->push_back(StreamOptions::Constraint(
        mandatory_constraints[i].m_name.utf8(),
        mandatory_constraints[i].m_value.utf8()));
  }

  blink::WebVector<blink::WebMediaConstraint> optional_constraints;
  constraints.getOptionalConstraints(optional_constraints);
  for (size_t i = 0; i < optional_constraints.size(); i++) {
    optional->push_back(StreamOptions::Constraint(
        optional_constraints[i].m_name.utf8(),
        optional_constraints[i].m_value.utf8()));
  }
}

static int g_next_request_id  = 0;

webrtc::MediaStreamInterface* GetNativeMediaStream(
    const blink::WebMediaStream& web_stream) {
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

void RemoveSource(const blink::WebMediaStreamSource& source,
                  std::vector<blink::WebMediaStreamSource>* sources) {
  for (std::vector<blink::WebMediaStreamSource>::iterator it =
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
    const blink::WebUserMediaRequest& user_media_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK(CalledOnValidThread());
  int request_id = g_next_request_id++;
  StreamOptions options;
  blink::WebFrame* frame = NULL;
  GURL security_origin;
  bool enable_automatic_output_device_selection = false;

  // |user_media_request| can't be mocked. So in order to test at all we check
  // if it isNull.
  if (user_media_request.isNull()) {
    // We are in a test.
    options.audio_requested = true;
    options.video_requested = true;
  } else {
    if (user_media_request.audio()) {
      options.audio_requested = true;
      CopyStreamConstraints(user_media_request.audioConstraints(),
                            &options.mandatory_audio,
                            &options.optional_audio);

      // Check if this input device should be used to select a matching output
      // device for audio rendering.
      std::string enable;
      if (options.GetFirstAudioConstraintByName(
              kMediaStreamRenderToAssociatedSink, &enable, NULL) &&
          LowerCaseEqualsASCII(enable, "true")) {
        enable_automatic_output_device_selection = true;
      }
    }
    if (user_media_request.video()) {
      options.video_requested = true;
      CopyStreamConstraints(user_media_request.videoConstraints(),
                            &options.mandatory_video,
                            &options.optional_video);
    }

    security_origin = GURL(user_media_request.securityOrigin().toString());
    // Get the WebFrame that requested a MediaStream.
    // The frame is needed to tell the MediaStreamDispatcher when a stream goes
    // out of scope.
    frame = user_media_request.ownerDocument().frame();
    DCHECK(frame);
  }

  DVLOG(1) << "MediaStreamImpl::requestUserMedia(" << request_id << ", [ "
           << "audio=" << (options.audio_requested)
           << " select associated sink: "
           << enable_automatic_output_device_selection
           << ", video=" << (options.video_requested) << " ], "
           << security_origin.spec() << ")";

  std::string audio_device_id;
  bool mandatory_audio;
  options.GetFirstAudioConstraintByName(kMediaStreamSourceInfoId,
                                        &audio_device_id, &mandatory_audio);
  std::string video_device_id;
  bool mandatory_video;
  options.GetFirstVideoConstraintByName(kMediaStreamSourceInfoId,
                                        &video_device_id, &mandatory_video);

  WebRtcLogMessage(base::StringPrintf(
      "MSI::requestUserMedia. request_id=%d"
      ", audio source id=%s mandatory= %s "
      ", video source id=%s mandatory= %s",
      request_id,
      audio_device_id.c_str(),
      mandatory_audio ? "true":"false",
      video_device_id.c_str(),
      mandatory_video ? "true":"false"));

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
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  UserMediaRequestInfo* request = FindUserMediaRequestInfo(user_media_request);
  if (request) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    DeleteUserMediaRequestInfo(request);
  }
}

blink::WebMediaStream MediaStreamImpl::GetMediaStream(
    const GURL& url) {
  return blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);
}

bool MediaStreamImpl::IsMediaStream(const GURL& url) {
  blink::WebMediaStream web_stream(
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));

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
  blink::WebMediaStream web_stream(GetMediaStream(url));

  if (web_stream.isNull() || !web_stream.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetVideoFrameProvider stream:"
           << UTF16ToUTF8(web_stream.id());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty())
    return NULL;

  return new RTCVideoRenderer(video_tracks[0], error_cb, repaint_cb);
}

scoped_refptr<MediaStreamAudioRenderer>
MediaStreamImpl::GetAudioRenderer(const GURL& url) {
  DCHECK(CalledOnValidThread());
  blink::WebMediaStream web_stream(GetMediaStream(url));

  if (web_stream.isNull() || !web_stream.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetAudioRenderer stream:"
           << UTF16ToUTF8(web_stream.id());

  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(web_stream.extraData());

  if (extra_data->is_local()) {
    // Create the local audio renderer if the stream contains audio tracks.
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    web_stream.audioTracks(audio_tracks);
    if (audio_tracks.isEmpty())
      return NULL;

    // TODO(xians): Add support for the case that the media stream contains
    // multiple audio tracks.
    return CreateLocalAudioRenderer(audio_tracks[0]);
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

  blink::WebVector<blink::WebMediaStreamSource> audio_source_vector(
        audio_array.size());

  // Log the device names for this request.
  for (StreamDeviceInfoArray::const_iterator it = audio_array.begin();
       it != audio_array.end(); ++it) {
    WebRtcLogMessage(base::StringPrintf(
        "Generated media stream for request id %d contains audio device name"
        " \"%s\"",
        request_id,
        it->device.name.c_str()));
  }

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
                           blink::WebMediaStreamSource::TypeAudio,
                           request_info->frame,
                           audio_source_vector);

  blink::WebVector<blink::WebMediaStreamSource> video_source_vector(
      video_array.size());
  CreateWebKitSourceVector(label, video_array,
                           blink::WebMediaStreamSource::TypeVideo,
                           request_info->frame,
                           video_source_vector);
  blink::WebUserMediaRequest* request = &(request_info->request);
  blink::WebString webkit_id = UTF8ToUTF16(label);
  blink::WebMediaStream* web_stream = &(request_info->web_stream);

  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  for (size_t i = 0; i < audio_track_vector.size(); ++i) {
    audio_track_vector[i].initialize(audio_source_vector[i]);
    request_info->sources.push_back(audio_source_vector[i]);
  }

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  for (size_t i = 0; i < video_track_vector.size(); ++i) {
    video_track_vector[i].initialize(video_source_vector[i]);
    request_info->sources.push_back(video_source_vector[i]);
  }

  web_stream->initialize(webkit_id, audio_track_vector,
                         video_track_vector);

  // WebUserMediaRequest don't have an implementation in unit tests.
  // Therefore we need to check for isNull here.
  blink::WebMediaConstraints audio_constraints = request->isNull() ?
      blink::WebMediaConstraints() : request->audioConstraints();
  blink::WebMediaConstraints video_constraints = request->isNull() ?
      blink::WebMediaConstraints() : request->videoConstraints();

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
// The browser process has stopped a device used by a MediaStream.
void MediaStreamImpl::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnDeviceStopped("
           << "{device_id = " << device_info.device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device_info);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happen stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }
  // By creating |source| it is guaranteed that the blink::WebMediaStreamSource
  // object is valid during the cleanup.
  blink::WebMediaStreamSource source(*source_ptr);
  StopLocalSource(source, false);

  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (device_it->source.id() == source.id()) {
      local_sources_.erase(device_it);
      break;
    }
  }

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

void MediaStreamImpl::CreateWebKitSourceVector(
    const std::string& label,
    const StreamDeviceInfoArray& devices,
    blink::WebMediaStreamSource::Type type,
    blink::WebFrame* frame,
    blink::WebVector<blink::WebMediaStreamSource>& webkit_sources) {
  CHECK_EQ(devices.size(), webkit_sources.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    const blink::WebMediaStreamSource* existing_source =
        FindLocalSource(devices[i]);
    if (existing_source) {
      webkit_sources[i] = *existing_source;
      DVLOG(1) << "Source already exist. Reusing source with id "
               << webkit_sources[i]. id().utf8();
      continue;
    }
    webkit_sources[i].initialize(
        UTF8ToUTF16(devices[i].device.id),
        type,
        UTF8ToUTF16(devices[i].device.name));
    MediaStreamSourceExtraData* source_extra_data(
        new content::MediaStreamSourceExtraData(
            devices[i],
            base::Bind(&MediaStreamImpl::OnLocalSourceStop, AsWeakPtr())));
    // |source_extra_data| is owned by webkit_sources[i].
    webkit_sources[i].setExtraData(source_extra_data);
    local_sources_.push_back(LocalStreamSource(frame, webkit_sources[i]));
  }
}

// Callback from MediaStreamDependencyFactory when the sources in |web_stream|
// have been generated.
void MediaStreamImpl::OnCreateNativeSourcesComplete(
    blink::WebMediaStream* web_stream,
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
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest* request_info,
    bool request_succeeded) {
  if (request_succeeded) {
    request_info->requestSucceeded(stream);
  } else {
    request_info->requestFailed();
  }
}

const blink::WebMediaStreamSource* MediaStreamImpl::FindLocalSource(
    const StreamDeviceInfo& device) const {
  for (LocalStreamSources::const_iterator it = local_sources_.begin();
       it != local_sources_.end(); ++it) {
    MediaStreamSourceExtraData* extra_data =
        static_cast<MediaStreamSourceExtraData*>(
            it->source.extraData());
    const StreamDeviceInfo& active_device = extra_data->device_info();
    if (active_device.device.id == device.device.id &&
        active_device.device.type == device.device.type &&
        active_device.session_id == device.session_id) {
      return &it->source;
    }
  }
  return NULL;
}

bool MediaStreamImpl::FindSourceInRequests(
    const blink::WebMediaStreamSource& source) const {
  for (UserMediaRequests::const_iterator req_it = user_media_requests_.begin();
       req_it != user_media_requests_.end(); ++req_it) {
    const std::vector<blink::WebMediaStreamSource>& sources =
        (*req_it)->sources;
    for (std::vector<blink::WebMediaStreamSource>::const_iterator source_it =
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
    const blink::WebUserMediaRequest& request) {
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
    blink::WebMediaStream* web_stream) {
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

void MediaStreamImpl::FrameDetached(blink::WebFrame* frame) {
  // Do same thing as FrameWillClose.
  FrameWillClose(frame);
}

void MediaStreamImpl::FrameWillClose(blink::WebFrame* frame) {
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
    const blink::WebMediaStreamSource& source) {
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
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSourceExtraData* extra_data =
        static_cast<MediaStreamSourceExtraData*> (source.extraData());
  CHECK(extra_data);
  DVLOG(1) << "MediaStreamImpl::StopLocalSource("
           << "{device_id = " << extra_data->device_info().device.id << "})";

  if (source.type() == blink::WebMediaStreamSource::TypeAudio) {
    if (extra_data->GetAudioCapturer()) {
      extra_data->GetAudioCapturer()->Stop();
    }
  }

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(extra_data->device_info());

  blink::WebMediaStreamSource writable_source(source);
  writable_source.setReadyState(
      blink::WebMediaStreamSource::ReadyStateEnded);
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
    const blink::WebMediaStreamTrack& audio_track) {
  DVLOG(1) << "MediaStreamImpl::CreateLocalAudioRenderer";

  int session_id = 0, sample_rate = 0, buffer_size = 0;
  if (!GetAuthorizedDeviceInfoForAudioRenderer(&session_id,
                                               &sample_rate,
                                               &buffer_size)) {
    GetDefaultOutputDeviceParams(&sample_rate, &buffer_size);
  }

  // Create a new WebRtcLocalAudioRenderer instance and connect it to the
  // existing WebRtcAudioCapturer so that the renderer can use it as source.
  return new WebRtcLocalAudioRenderer(
      audio_track,
      RenderViewObserver::routing_id(),
      session_id,
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
    blink::WebFrame* frame,
    const blink::WebUserMediaRequest& request,
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
