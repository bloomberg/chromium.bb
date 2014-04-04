// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebUserMediaClientMock.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/MockConstraints.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
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

using namespace blink;

namespace WebTestRunner {

class UserMediaRequestTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request, const WebMediaStream result)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
        , m_result(result)
    {
        DCHECK(!m_result.isNull());
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_result);
    }

private:
    WebUserMediaRequest m_request;
    WebMediaStream m_result;
};

class UserMediaRequestConstraintFailedTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestConstraintFailedTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request, const WebString& constraint)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
        , m_constraint(constraint)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailedConstraint(m_constraint);
    }

private:
    WebUserMediaRequest m_request;
    WebString m_constraint;
};

class UserMediaRequestPermissionDeniedTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestPermissionDeniedTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailed();
    }

private:
    WebUserMediaRequest m_request;
};

class MediaDevicesRequestTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    MediaDevicesRequestTask(WebUserMediaClientMock* object, const WebMediaDevicesRequest& request, const WebVector<WebMediaDeviceInfo>& result)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_result);
    }

private:
    WebMediaDevicesRequest m_request;
    WebVector<WebMediaDeviceInfo> m_result;
};

////////////////////////////////

class MockExtraData : public WebMediaStream::ExtraData {
public:
    int foo;
};

WebUserMediaClientMock::WebUserMediaClientMock(WebTestDelegate* delegate)
    : m_delegate(delegate)
{
}

void WebUserMediaClientMock::requestUserMedia(const WebUserMediaRequest& streamRequest)
{
    DCHECK(!streamRequest.isNull());
    WebUserMediaRequest request = streamRequest;

    if (request.ownerDocument().isNull() || !request.ownerDocument().frame()) {
        m_delegate->postTask(new UserMediaRequestPermissionDeniedTask(this, request));
        return;
    }

    WebMediaConstraints constraints = request.audioConstraints();
    WebString failedConstraint;
    if (!constraints.isNull() && !MockConstraints::verifyConstraints(constraints, &failedConstraint)) {
        m_delegate->postTask(new UserMediaRequestConstraintFailedTask(this, request, failedConstraint));
        return;
    }
    constraints = request.videoConstraints();
    if (!constraints.isNull() && !MockConstraints::verifyConstraints(constraints, &failedConstraint)) {
        m_delegate->postTask(new UserMediaRequestConstraintFailedTask(this, request, failedConstraint));
        return;
    }

    const size_t zero = 0;
    const size_t one = 1;
    WebVector<WebMediaStreamTrack> audioTracks(request.audio() ? one : zero);
    WebVector<WebMediaStreamTrack> videoTracks(request.video() ? one : zero);

    if (request.audio()) {
        WebMediaStreamSource source;
        source.initialize("MockAudioDevice#1", WebMediaStreamSource::TypeAudio, "Mock audio device");
        audioTracks[0].initialize(source);
    }

    if (request.video()) {
        WebMediaStreamSource source;
        source.initialize("MockVideoDevice#1", WebMediaStreamSource::TypeVideo, "Mock video device");
        videoTracks[0].initialize(source);
    }

    WebMediaStream stream;
    stream.initialize(audioTracks, videoTracks);

    stream.setExtraData(new MockExtraData());

    m_delegate->postTask(new UserMediaRequestTask(this, request, stream));
}

void WebUserMediaClientMock::cancelUserMediaRequest(const WebUserMediaRequest&)
{
}

void WebUserMediaClientMock::requestMediaDevices(const WebMediaDevicesRequest& request)
{
    const size_t three = 3;
    WebVector<WebMediaDeviceInfo> devices(three);

    devices[0].initialize("device1", WebMediaDeviceInfo::MediaDeviceKindAudioInput, "Built-in microphone", "group1");
    devices[1].initialize("device2", WebMediaDeviceInfo::MediaDeviceKindAudioOutput, "Built-in speakers", "group1");
    devices[2].initialize("device3", WebMediaDeviceInfo::MediaDeviceKindVideoInput, "Build-in webcam", "group2");

    m_delegate->postTask(new MediaDevicesRequestTask(this, request, devices));
}

void WebUserMediaClientMock::cancelMediaDevicesRequest(const WebMediaDevicesRequest&)
{
}

}
