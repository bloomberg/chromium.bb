// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_web_user_media_client.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/mock_constraints.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebMediaDevicesRequest.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
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

namespace content {

class UserMediaRequestTask : public WebMethodTask<MockWebUserMediaClient> {
 public:
  UserMediaRequestTask(MockWebUserMediaClient* object,
                       const WebUserMediaRequest& request,
                       const WebMediaStream result)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        result_(result) {
    DCHECK(!result_.isNull());
  }

  virtual void runIfValid() OVERRIDE {
    request_.requestSucceeded(result_);
  }

 private:
  WebUserMediaRequest request_;
  WebMediaStream result_;
};

class UserMediaRequestConstraintFailedTask
    : public WebMethodTask<MockWebUserMediaClient> {
 public:
  UserMediaRequestConstraintFailedTask(MockWebUserMediaClient* object,
                                       const WebUserMediaRequest& request,
                                       const WebString& constraint)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        constraint_(constraint) {}

  virtual void runIfValid() OVERRIDE {
    request_.requestFailedConstraint(constraint_);
  }

 private:
  WebUserMediaRequest request_;
  WebString constraint_;
};

class UserMediaRequestPermissionDeniedTask
    : public WebMethodTask<MockWebUserMediaClient> {
 public:
  UserMediaRequestPermissionDeniedTask(MockWebUserMediaClient* object,
                                       const WebUserMediaRequest& request)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request) {}

  virtual void runIfValid() OVERRIDE {
    request_.requestFailed();
  }

 private:
  WebUserMediaRequest request_;
};

class MediaDevicesRequestTask : public WebMethodTask<MockWebUserMediaClient> {
 public:
  MediaDevicesRequestTask(MockWebUserMediaClient* object,
                          const WebMediaDevicesRequest& request,
                          const WebVector<WebMediaDeviceInfo>& result)
      : WebMethodTask<MockWebUserMediaClient>(object),
        request_(request),
        result_(result) {}

  virtual void runIfValid() OVERRIDE {
    request_.requestSucceeded(result_);
  }

 private:
  WebMediaDevicesRequest request_;
  WebVector<WebMediaDeviceInfo> result_;
};

class MockExtraData : public WebMediaStream::ExtraData {
 public:
  int foo;
};

MockWebUserMediaClient::MockWebUserMediaClient(WebTestDelegate* delegate)
    : delegate_(delegate) {}

void MockWebUserMediaClient::requestUserMedia(
    const WebUserMediaRequest& stream_request) {
    DCHECK(!stream_request.isNull());
    WebUserMediaRequest request = stream_request;

    if (request.ownerDocument().isNull() || !request.ownerDocument().frame()) {
      delegate_->postTask(
          new UserMediaRequestPermissionDeniedTask(this, request));
        return;
    }

    WebMediaConstraints constraints = request.audioConstraints();
    WebString failed_constraint;
    if (!constraints.isNull() &&
        !MockConstraints::VerifyConstraints(constraints, &failed_constraint)) {
      delegate_->postTask(new UserMediaRequestConstraintFailedTask(
          this, request, failed_constraint));
      return;
    }
    constraints = request.videoConstraints();
    if (!constraints.isNull() &&
        !MockConstraints::VerifyConstraints(constraints, &failed_constraint)) {
      delegate_->postTask(new UserMediaRequestConstraintFailedTask(
          this, request, failed_constraint));
      return;
    }

    const size_t zero = 0;
    const size_t one = 1;
    WebVector<WebMediaStreamTrack> audio_tracks(request.audio() ? one : zero);
    WebVector<WebMediaStreamTrack> video_tracks(request.video() ? one : zero);

    if (request.audio()) {
      WebMediaStreamSource source;
      source.initialize("MockAudioDevice#1",
                        WebMediaStreamSource::TypeAudio,
                        "Mock audio device");
      audio_tracks[0].initialize(source);
    }

    if (request.video()) {
      WebMediaStreamSource source;
      source.initialize("MockVideoDevice#1",
                        WebMediaStreamSource::TypeVideo,
                        "Mock video device");
      video_tracks[0].initialize(source);
    }

    WebMediaStream stream;
    stream.initialize(audio_tracks, video_tracks);

    stream.setExtraData(new MockExtraData());

    delegate_->postTask(new UserMediaRequestTask(this, request, stream));
}

void MockWebUserMediaClient::cancelUserMediaRequest(
    const WebUserMediaRequest&) {
}

void MockWebUserMediaClient::requestMediaDevices(
    const WebMediaDevicesRequest& request) {
  const size_t three = 3;
  WebVector<WebMediaDeviceInfo> devices(three);

  devices[0].initialize("device1",
                        WebMediaDeviceInfo::MediaDeviceKindAudioInput,
                        "Built-in microphone",
                        "group1");
  devices[1].initialize("device2",
                        WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
                        "Built-in speakers",
                        "group1");
  devices[2].initialize("device3",
                        WebMediaDeviceInfo::MediaDeviceKindVideoInput,
                        "Build-in webcam",
                        "group2");

  delegate_->postTask(new MediaDevicesRequestTask(this, request, devices));
}

void MockWebUserMediaClient::cancelMediaDevicesRequest(
    const WebMediaDevicesRequest&) {
}

}  // namespace content
