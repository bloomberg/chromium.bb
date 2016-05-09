// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_web_user_media_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebMediaDevicesRequest.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

using blink::WebMediaConstraints;
using blink::WebMediaDeviceInfo;
using blink::WebMediaDevicesRequest;
using blink::WebMediaStream;
using blink::WebMediaStreamSource;
using blink::WebMediaStreamTrack;
using blink::WebMediaStreamTrackSourcesRequest;
using blink::WebSourceInfo;
using blink::WebString;
using blink::WebUserMediaRequest;
using blink::WebVector;

namespace test_runner {

class MockExtraData : public WebMediaStream::ExtraData {
};

MockWebUserMediaClient::MockWebUserMediaClient(WebTestDelegate* delegate)
    : delegate_(delegate),
      should_enumerate_extra_device_(false),
      weak_factory_(this) {}

MockWebUserMediaClient::~MockWebUserMediaClient() {}

void MockWebUserMediaClient::requestUserMedia(
    const WebUserMediaRequest& stream_request) {
    DCHECK(!stream_request.isNull());
    WebUserMediaRequest request = stream_request;

    if (request.ownerDocument().isNull() || !request.ownerDocument().frame()) {
      delegate_->PostTask(new WebCallbackTask(base::Bind(
          &WebUserMediaRequest::requestFailed,
          base::Owned(new WebUserMediaRequest(request)), WebString())));
      return;
    }

    WebMediaStream stream;
    stream.initialize(WebVector<WebMediaStreamTrack>(),
                      WebVector<WebMediaStreamTrack>());
    stream.setExtraData(new MockExtraData());

    if (request.audio() &&
        !delegate_->AddMediaStreamAudioSourceAndTrack(&stream)) {
      WebMediaStreamSource source;
      source.initialize("MockAudioDevice#1",
                        WebMediaStreamSource::TypeAudio,
                        "Mock audio device",
                        false /* remote */);
      WebMediaStreamTrack web_track;
      web_track.initialize(source);
      stream.addTrack(web_track);
    }

    if (request.video() &&
        !delegate_->AddMediaStreamVideoSourceAndTrack(&stream)) {
      WebMediaStreamSource source;
      source.initialize("MockVideoDevice#1",
                        WebMediaStreamSource::TypeVideo,
                        "Mock video device",
                        false /* remote */);
      WebMediaStreamTrack web_track;
      web_track.initialize(source);
      stream.addTrack(web_track);
    }

    delegate_->PostTask(new WebCallbackTask(
        base::Bind(&WebUserMediaRequest::requestSucceeded,
                   base::Owned(new WebUserMediaRequest(request)), stream)));
}

void MockWebUserMediaClient::cancelUserMediaRequest(
    const WebUserMediaRequest&) {
}

void MockWebUserMediaClient::requestMediaDevices(
    const WebMediaDevicesRequest& request) {
  struct {
    const char* device_id;
    WebMediaDeviceInfo::MediaDeviceKind kind;
    const char* label;
    const char* group_id;
  } test_devices[] = {
    {
      "device1",
      WebMediaDeviceInfo::MediaDeviceKindAudioInput,
      "Built-in microphone",
      "group1",
    },
    {
      "device2",
      WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
      "Built-in speakers",
      "group1",
    },
    {
      "device3",
      WebMediaDeviceInfo::MediaDeviceKindVideoInput,
      "Built-in webcam",
      "group2",
    },
    {
      "device4",
      WebMediaDeviceInfo::MediaDeviceKindAudioInput,
      "Extra microphone",
      "group3",
    },
  };

  size_t num_devices = should_enumerate_extra_device_
                           ? arraysize(test_devices)
                           : arraysize(test_devices) - 1;
  WebVector<WebMediaDeviceInfo> devices(num_devices);
  for (size_t i = 0; i < num_devices; ++i) {
    devices[i].initialize(WebString::fromUTF8(test_devices[i].device_id),
                          test_devices[i].kind,
                          WebString::fromUTF8(test_devices[i].label),
                          WebString::fromUTF8(test_devices[i].group_id));
  }

  delegate_->PostTask(new WebCallbackTask(
      base::Bind(&WebMediaDevicesRequest::requestSucceeded,
                 base::Owned(new WebMediaDevicesRequest(request)), devices)));

  should_enumerate_extra_device_ = !should_enumerate_extra_device_;
  if (!media_device_change_observer_.isNull())
    media_device_change_observer_.didChangeMediaDevices();
}

void MockWebUserMediaClient::cancelMediaDevicesRequest(
    const WebMediaDevicesRequest&) {
}

void MockWebUserMediaClient::requestSources(
    const blink::WebMediaStreamTrackSourcesRequest& request) {
  struct {
    const char* id;
    WebSourceInfo::SourceKind kind;
    const char* label;
    WebSourceInfo::VideoFacingMode facing;
  } test_sources[] = {
    {
      "device1",
      WebSourceInfo::SourceKindAudio,
      "Built-in microphone",
      WebSourceInfo::VideoFacingModeNone,
    },
    {
      "device2",
      WebSourceInfo::SourceKindVideo,
      "Build-in webcam",
      WebSourceInfo::VideoFacingModeEnvironment,
    },
  };

  WebVector<WebSourceInfo> sources(arraysize(test_sources));
  for (size_t i = 0; i < arraysize(test_sources); ++i) {
  sources[i].initialize(WebString::fromUTF8(test_sources[i].id),
                        test_sources[i].kind,
                        WebString::fromUTF8(test_sources[i].label),
                        test_sources[i].facing);
  }

  delegate_->PostTask(new WebCallbackTask(base::Bind(
      &WebMediaStreamTrackSourcesRequest::requestSucceeded,
      base::Owned(new WebMediaStreamTrackSourcesRequest(request)), sources)));
}

void MockWebUserMediaClient::setMediaDeviceChangeObserver(
    const blink::WebMediaDeviceChangeObserver& observer) {
  media_device_change_observer_ = observer;
}

}  // namespace test_runner
