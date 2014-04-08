// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
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

void GetDefaultOutputDeviceParams(
    int* output_sample_rate, int* output_buffer_size) {
  // Fetch the default audio output hardware config.
  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();
  *output_sample_rate = hardware_config->GetOutputSampleRate();
  *output_buffer_size = hardware_config->GetOutputBufferSize();
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

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        user_media_request);
  }

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

  return (!web_stream.isNull() &&
      (MediaStream::GetMediaStream(web_stream) != NULL));
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
           << base::UTF16ToUTF8(web_stream.id());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty() ||
      !MediaStreamVideoTrack::GetTrack(video_tracks[0])) {
    return NULL;
  }

  return new RTCVideoRenderer(video_tracks[0], error_cb, repaint_cb);
}

scoped_refptr<MediaStreamAudioRenderer>
MediaStreamImpl::GetAudioRenderer(const GURL& url, int render_frame_id) {
  DCHECK(CalledOnValidThread());
  blink::WebMediaStream web_stream(GetMediaStream(url));

  if (web_stream.isNull() || !web_stream.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamImpl::GetAudioRenderer stream:"
           << base::UTF16ToUTF8(web_stream.id());

  MediaStream* native_stream = MediaStream::GetMediaStream(web_stream);

  // TODO(tommi): MediaStreams do not have a 'local or not' concept.
  // Tracks _might_, but even so, we need to fix the data flow so that
  // it works the same way for all track implementations, local, remote or what
  // have you.
  // In this function, we should simply create a renderer object that receives
  // and mixes audio from all the tracks that belong to the media stream.
  // We need to remove the |is_local| property from MediaStreamExtraData since
  // this concept is peerconnection specific (is a previously recorded stream
  // local or remote?).
  if (native_stream->is_local()) {
    // Create the local audio renderer if the stream contains audio tracks.
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    web_stream.audioTracks(audio_tracks);
    if (audio_tracks.isEmpty())
      return NULL;

    // TODO(xians): Add support for the case where the media stream contains
    // multiple audio tracks.
    return CreateLocalAudioRenderer(audio_tracks[0], render_frame_id);
  }

  webrtc::MediaStreamInterface* stream =
      MediaStream::GetAdapter(web_stream);
  if (stream->GetAudioTracks().empty())
    return NULL;

  // This is a remote WebRTC media stream.
  WebRtcAudioDeviceImpl* audio_device =
      dependency_factory_->GetWebRtcAudioDevice();

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (!renderer.get()) {
    renderer = CreateRemoteAudioRenderer(stream, render_frame_id);

    if (renderer.get() && !audio_device->SetAudioRenderer(renderer.get()))
      renderer = NULL;
  }

  return renderer.get() ?
      renderer->CreateSharedAudioRendererProxy(stream) : NULL;
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

  // WebUserMediaRequest don't have an implementation in unit tests.
  // Therefore we need to check for isNull here and initialize the
  // constraints.
  blink::WebUserMediaRequest* request = &(request_info->request);
  blink::WebMediaConstraints audio_constraints;
  blink::WebMediaConstraints video_constraints;
  if (request->isNull()) {
    audio_constraints.initialize();
    video_constraints.initialize();
  } else {
    audio_constraints = request->audioConstraints();
    video_constraints = request->videoConstraints();
  }

  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  CreateAudioTracks(audio_array, audio_constraints, &audio_track_vector,
                    request_info);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  CreateVideoTracks(video_array, video_constraints, &video_track_vector,
                    request_info);

  blink::WebString webkit_id = base::UTF8ToUTF16(label);
  blink::WebMediaStream* web_stream = &(request_info->web_stream);

  web_stream->initialize(webkit_id, audio_track_vector,
                         video_track_vector);
  web_stream->setExtraData(
      new MediaStream(
          dependency_factory_,
          base::Bind(&MediaStreamImpl::OnLocalMediaStreamStop, AsWeakPtr()),
          *web_stream));

  // Wait for the tracks to be started successfully or to fail.
  request_info->CallbackOnTracksStarted(
      base::Bind(&MediaStreamImpl::OnCreateNativeTracksCompleted, AsWeakPtr()));
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void MediaStreamImpl::OnStreamGenerationFailed(
    int request_id,
    content::MediaStreamRequestResult result) {
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
                              result);
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
    (*it)->RemoveSource(source);
    if ((*it)->AreAllSourcesRemoved()) {
      it = user_media_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

void MediaStreamImpl::InitializeSourceObject(
    const StreamDeviceInfo& device,
    blink::WebMediaStreamSource::Type type,
    const blink::WebMediaConstraints& constraints,
    blink::WebFrame* frame,
    blink::WebMediaStreamSource* webkit_source) {
  const blink::WebMediaStreamSource* existing_source =
      FindLocalSource(device);
  if (existing_source) {
    *webkit_source = *existing_source;
    DVLOG(1) << "Source already exist. Reusing source with id "
             << webkit_source->id().utf8();
    return;
  }

  webkit_source->initialize(
      base::UTF8ToUTF16(device.device.id),
      type,
      base::UTF8ToUTF16(device.device.name));

  DVLOG(1) << "Initialize source object :"
           << "id = " << webkit_source->id().utf8()
           << ", name = " << webkit_source->name().utf8();

  if (type == blink::WebMediaStreamSource::TypeVideo) {
    webkit_source->setExtraData(
        CreateVideoSource(
            device,
            base::Bind(&MediaStreamImpl::OnLocalSourceStopped, AsWeakPtr())));
  } else {
    DCHECK_EQ(blink::WebMediaStreamSource::TypeAudio, type);
    MediaStreamAudioSource* audio_source(
        new MediaStreamAudioSource(
            RenderViewObserver::routing_id(),
            device,
            base::Bind(&MediaStreamImpl::OnLocalSourceStopped, AsWeakPtr()),
            dependency_factory_));
    webkit_source->setExtraData(audio_source);
  }
  local_sources_.push_back(LocalStreamSource(frame, *webkit_source));
}

MediaStreamVideoSource* MediaStreamImpl::CreateVideoSource(
    const StreamDeviceInfo& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  return new content::MediaStreamVideoCapturerSource(
      device,
      stop_callback,
      new VideoCapturerDelegate(device),
      dependency_factory_);
}

void MediaStreamImpl::CreateVideoTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource webkit_source;
    InitializeSourceObject(devices[i],
                           blink::WebMediaStreamSource::TypeVideo,
                           constraints,
                           request->frame,
                           &webkit_source);
    (*webkit_tracks)[i] =
        request->CreateAndStartVideoTrack(webkit_source, constraints,
                                          dependency_factory_);
  }
}

void MediaStreamImpl::CreateAudioTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  // Log the device names for this request.
  for (StreamDeviceInfoArray::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    WebRtcLogMessage(base::StringPrintf(
        "Generated media stream for request id %d contains audio device name"
        " \"%s\"",
        request->request_id,
        it->device.name.c_str()));
  }

  StreamDeviceInfoArray overridden_audio_array = devices;
  if (!request->enable_automatic_output_device_selection) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (StreamDeviceInfoArray::iterator it = overridden_audio_array.begin();
         it != overridden_audio_array.end(); ++it) {
      it->device.matched_output_device_id = "";
      it->device.matched_output = MediaStreamDevice::AudioDeviceParameters();
    }
  }

  for (size_t i = 0; i < overridden_audio_array.size(); ++i) {
    blink::WebMediaStreamSource webkit_source;
    InitializeSourceObject(overridden_audio_array[i],
                           blink::WebMediaStreamSource::TypeAudio,
                           constraints,
                           request->frame,
                           &webkit_source);
    (*webkit_tracks)[i].initialize(webkit_source);
    request->StartAudioTrack((*webkit_tracks)[i], constraints);
  }
}

void MediaStreamImpl::OnCreateNativeTracksCompleted(
    UserMediaRequestInfo* request,
    content::MediaStreamRequestResult result) {
  DVLOG(1) << "MediaStreamImpl::OnCreateNativeTracksComplete("
           << "{request_id = " << request->request_id << "} "
           << "{result = " << result << "})";
  CompleteGetUserMediaRequest(request->web_stream, &request->request,
                              result);
  if (result != MEDIA_DEVICE_OK) {
    // TODO(perkj): Once we don't support MediaStream::Stop the |request_info|
    // can be deleted even if the request succeeds.
    DeleteUserMediaRequestInfo(request);
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
    content::MediaStreamRequestResult result) {

  DVLOG(1) << "MediaStreamImpl::CompleteGetUserMediaRequest("
           << "result=" << result;

  switch (result) {
    case MEDIA_DEVICE_OK:
      request_info->requestSucceeded(stream);
      break;
    case MEDIA_DEVICE_PERMISSION_DENIED:
      request_info->requestDenied();
      break;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      request_info->requestFailedUASpecific("PermissionDismissedError");
      break;
    case MEDIA_DEVICE_INVALID_STATE:
      request_info->requestFailedUASpecific("InvalidStateError");
      break;
    case MEDIA_DEVICE_NO_HARDWARE:
      request_info->requestFailedUASpecific("DevicesNotFoundError");
      break;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      request_info->requestFailedUASpecific("InvalidSecurityOriginError");
      break;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      request_info->requestFailedUASpecific("TabCaptureError");
      break;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      request_info->requestFailedUASpecific("ScreenCaptureError");
      break;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      request_info->requestFailedUASpecific("DeviceCaptureError");
      break;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      request_info->requestFailedUASpecific("TrackStartError");
      break;
    default:
      request_info->requestFailed();
      break;
  }
}

const blink::WebMediaStreamSource* MediaStreamImpl::FindLocalSource(
    const StreamDeviceInfo& device) const {
  for (LocalStreamSources::const_iterator it = local_sources_.begin();
       it != local_sources_.end(); ++it) {
    MediaStreamSource* source =
        static_cast<MediaStreamSource*>(it->source.extraData());
    const StreamDeviceInfo& active_device = source->device_info();
    if (active_device.device.id == device.device.id &&
        active_device.device.type == device.device.type &&
        active_device.session_id == device.session_id) {
      return &it->source;
    }
  }
  return NULL;
}

bool MediaStreamImpl::IsSourceInRequests(
    const blink::WebMediaStreamSource& source) const {
  for (UserMediaRequests::const_iterator req_it = user_media_requests_.begin();
       req_it != user_media_requests_.end(); ++req_it) {
    if ((*req_it)->IsSourceUsed(source))
      return true;
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
    if ((*it)->generated && (*it)->web_stream.id() == base::UTF8ToUTF16(label))
      return (*it);
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

void MediaStreamImpl::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());

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
    (*it)->RemoveSource(source);
    if ((*it)->AreAllSourcesRemoved()) {
      it = user_media_requests_.erase(it);
    } else {
      ++it;
    }
  }

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*> (source.extraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());
}

void MediaStreamImpl::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*> (source.extraData());
  DVLOG(1) << "MediaStreamImpl::StopLocalSource("
           << "{device_id = " << source_impl->device_info().device.id << "})";

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

void MediaStreamImpl::StopUnreferencedSources(bool notify_dispatcher) {
  LocalStreamSources::iterator source_it = local_sources_.begin();
  while (source_it != local_sources_.end()) {
    if (!IsSourceInRequests(source_it->source)) {
      StopLocalSource(source_it->source, notify_dispatcher);
      source_it = local_sources_.erase(source_it);
    } else {
      ++source_it;
    }
  }
}

scoped_refptr<WebRtcAudioRenderer> MediaStreamImpl::CreateRemoteAudioRenderer(
    webrtc::MediaStreamInterface* stream,
    int render_frame_id) {
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

  return new WebRtcAudioRenderer(
      stream, RenderViewObserver::routing_id(), render_frame_id,  session_id,
      sample_rate, buffer_size);
}

scoped_refptr<WebRtcLocalAudioRenderer>
MediaStreamImpl::CreateLocalAudioRenderer(
    const blink::WebMediaStreamTrack& audio_track,
    int render_frame_id) {
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
      render_frame_id,
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

  return audio_device->GetAuthorizedDeviceInfoForAudioRenderer(
      session_id, output_sample_rate, output_frames_per_buffer);
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
      request(request),
      request_failed_(false) {
}

MediaStreamImpl::UserMediaRequestInfo::~UserMediaRequestInfo() {
  DVLOG(1) << "~UserMediaRequestInfo";
}

void MediaStreamImpl::UserMediaRequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(track.source().type() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* native_source =
      static_cast <MediaStreamAudioSource*>(track.source().extraData());
  DCHECK(native_source);

  sources_.push_back(track.source());
  sources_waiting_for_callback_.push_back(native_source);
  native_source->AddTrack(
      track, constraints, base::Bind(
          &MediaStreamImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()));
}

blink::WebMediaStreamTrack
MediaStreamImpl::UserMediaRequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source,
    const blink::WebMediaConstraints& constraints,
    MediaStreamDependencyFactory* factory) {
  DCHECK(source.type() == blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, constraints, base::Bind(
          &MediaStreamImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()),
      true, factory);
}

void MediaStreamImpl::UserMediaRequestInfo::CallbackOnTracksStarted(
    const ResourcesReady& callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = callback;
  CheckAllTracksStarted();
}

void MediaStreamImpl::UserMediaRequestInfo::OnTrackStarted(
    MediaStreamSource* source, bool success) {
  DVLOG(1) << "OnTrackStarted";
  std::vector<MediaStreamSource*>::iterator it =
      std::find(sources_waiting_for_callback_.begin(),
                sources_waiting_for_callback_.end(),
                source);
  DCHECK(it != sources_waiting_for_callback_.end());
  sources_waiting_for_callback_.erase(it);
  // All tracks must be started successfully. Otherwise the request is a
  // failure.
  if (!success)
    request_failed_ = true;
  CheckAllTracksStarted();
}

void MediaStreamImpl::UserMediaRequestInfo::CheckAllTracksStarted() {
  if (!ready_callback_.is_null() && sources_waiting_for_callback_.empty()) {
    ready_callback_.Run(
        this,
        request_failed_ ? MEDIA_DEVICE_TRACK_START_FAILURE : MEDIA_DEVICE_OK);
  }
}

bool MediaStreamImpl::UserMediaRequestInfo::IsSourceUsed(
    const blink::WebMediaStreamSource& source) const {
  for (std::vector<blink::WebMediaStreamSource>::const_iterator source_it =
           sources_.begin();
       source_it != sources_.end(); ++source_it) {
    if (source_it->id() == source.id())
      return true;
  }
  return false;
}

void MediaStreamImpl::UserMediaRequestInfo::RemoveSource(
    const blink::WebMediaStreamSource& source) {
  for (std::vector<blink::WebMediaStreamSource>::iterator it =
           sources_.begin();
       it != sources_.end(); ++it) {
    if (source.id() == it->id()) {
      sources_.erase(it);
      return;
    }
  }
}

}  // namespace content
