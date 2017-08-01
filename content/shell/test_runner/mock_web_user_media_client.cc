// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_web_user_media_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
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
using blink::WebString;
using blink::WebUserMediaRequest;
using blink::WebVector;

namespace test_runner {

class MockExtraData : public WebMediaStream::ExtraData {};

MockWebUserMediaClient::MockWebUserMediaClient(WebTestDelegate* delegate)
    : delegate_(delegate),
      should_enumerate_extra_device_(false),
      weak_factory_(this) {}

MockWebUserMediaClient::~MockWebUserMediaClient() {}

void MockWebUserMediaClient::RequestUserMedia(
    const WebUserMediaRequest& stream_request) {
  DCHECK(!stream_request.IsNull());
  WebUserMediaRequest request = stream_request;

  if (request.OwnerDocument().IsNull() || !request.OwnerDocument().GetFrame()) {
    delegate_->PostTask(
        base::Bind(&WebUserMediaRequest::RequestFailed,
                   base::Owned(new WebUserMediaRequest(request)), WebString()));
    return;
  }

  WebMediaStream stream;
  stream.Initialize(WebVector<WebMediaStreamTrack>(),
                    WebVector<WebMediaStreamTrack>());
  stream.SetExtraData(new MockExtraData());

  if (request.Audio() &&
      !delegate_->AddMediaStreamAudioSourceAndTrack(&stream)) {
    WebMediaStreamSource source;
    source.Initialize("MockAudioDevice#1", WebMediaStreamSource::kTypeAudio,
                      "Mock audio device", false /* remote */);
    WebMediaStreamTrack web_track;
    web_track.Initialize(source);
    stream.AddTrack(web_track);
  }

  if (request.Video() &&
      !delegate_->AddMediaStreamVideoSourceAndTrack(&stream)) {
    WebMediaStreamSource source;
    source.Initialize("MockVideoDevice#1", WebMediaStreamSource::kTypeVideo,
                      "Mock video device", false /* remote */);
    WebMediaStreamTrack web_track;
    web_track.Initialize(source);
    stream.AddTrack(web_track);
  }

  delegate_->PostTask(base::Bind(&WebUserMediaRequest::RequestSucceeded,
                                 base::Owned(new WebUserMediaRequest(request)),
                                 stream));
}

void MockWebUserMediaClient::CancelUserMediaRequest(
    const WebUserMediaRequest&) {}

void MockWebUserMediaClient::RequestMediaDevices(
    const WebMediaDevicesRequest& request) {
  struct {
    const char* device_id;
    WebMediaDeviceInfo::MediaDeviceKind kind;
    const char* label;
    const char* group_id;
  } test_devices[] = {
      {
          "device1", WebMediaDeviceInfo::kMediaDeviceKindAudioInput,
          "Built-in microphone", "group1",
      },
      {
          "device2", WebMediaDeviceInfo::kMediaDeviceKindAudioOutput,
          "Built-in speakers", "group1",
      },
      {
          "device3", WebMediaDeviceInfo::kMediaDeviceKindVideoInput,
          "Built-in webcam", "group2",
      },
      {
          "device4", WebMediaDeviceInfo::kMediaDeviceKindAudioInput,
          "Extra microphone", "group3",
      },
  };

  size_t num_devices = should_enumerate_extra_device_
                           ? arraysize(test_devices)
                           : arraysize(test_devices) - 1;
  WebVector<WebMediaDeviceInfo> devices(num_devices);
  for (size_t i = 0; i < num_devices; ++i) {
    devices[i].Initialize(WebString::FromUTF8(test_devices[i].device_id),
                          test_devices[i].kind,
                          WebString::FromUTF8(test_devices[i].label),
                          WebString::FromUTF8(test_devices[i].group_id));
  }

  delegate_->PostTask(
      base::Bind(&WebMediaDevicesRequest::RequestSucceeded,
                 base::Owned(new WebMediaDevicesRequest(request)), devices));

  should_enumerate_extra_device_ = !should_enumerate_extra_device_;
  if (!media_device_change_observer_.IsNull())
    media_device_change_observer_.DidChangeMediaDevices();
}

void MockWebUserMediaClient::SetMediaDeviceChangeObserver(
    const blink::WebMediaDeviceChangeObserver& observer) {
  media_device_change_observer_ = observer;
}

}  // namespace test_runner
