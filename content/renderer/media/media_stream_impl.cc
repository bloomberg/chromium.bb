// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/capture_video_decoder.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
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
}  // namespace

static int g_next_request_id  = 0;

// Creates a WebKit representation of a stream sources based on
// |devices| from the MediaStreamDispatcher.
static void CreateWebKitSourceVector(
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& devices,
    WebKit::WebMediaStreamSource::Type type,
    WebKit::WebVector<WebKit::WebMediaStreamSource>& webkit_sources) {
  CHECK_EQ(devices.size(), webkit_sources.size());
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
    VideoCaptureImplManager* vc_manager,
    MediaStreamDependencyFactory* dependency_factory)
    : content::RenderViewObserver(render_view),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher),
      vc_manager_(vc_manager) {
}

MediaStreamImpl::~MediaStreamImpl() {
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

  if (!dependency_factory_->CreateNativeLocalMediaStream(&description)) {
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
