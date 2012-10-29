// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/rtc_media_constraints.h"

#include "base/logging.h"

#include "content/common/media/media_stream_options.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

namespace content {
namespace {

void GetNativeMediaConstraints(
    const WebKit::WebVector<WebKit::WebMediaConstraint>& constraints,
    webrtc::MediaConstraintsInterface::Constraints* native_constraints) {
  DCHECK(native_constraints);
  for (size_t i = 0; i < constraints.size(); ++i) {
    webrtc::MediaConstraintsInterface::Constraint new_constraint;
    new_constraint.key = constraints[i].m_name.utf8();
    new_constraint.value = constraints[i].m_value.utf8();

    // Ignore Chrome specific Tab capture constraints.
    if (new_constraint.key == kMediaStreamSource ||
        new_constraint.key == kMediaStreamSourceId)
      continue;
    DVLOG(3) << "MediaStreamConstraints:" << new_constraint.key
             << " : " <<  new_constraint.value;
    native_constraints->push_back(new_constraint);
  }
}

}  // namespace

RTCMediaConstraints::RTCMediaConstraints(
      const WebKit::WebMediaConstraints& constraints) {
  if (constraints.isNull())
    return;  // Will happen in unit tests.
  WebKit::WebVector<WebKit::WebMediaConstraint> mandatory;
  constraints.getMandatoryConstraints(mandatory);
  GetNativeMediaConstraints(mandatory, &mandatory_);
  WebKit::WebVector<WebKit::WebMediaConstraint> optional;
  constraints.getOptionalConstraints(optional);
  GetNativeMediaConstraints(optional, &optional_);
}

RTCMediaConstraints::~RTCMediaConstraints() {}

const webrtc::MediaConstraintsInterface::Constraints&
RTCMediaConstraints::GetMandatory() const  {
  return mandatory_;
}

const webrtc::MediaConstraintsInterface::Constraints&
RTCMediaConstraints::GetOptional() const {
  return optional_;
}

}  // namespace content
