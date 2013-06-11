// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/rtc_media_constraints.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/common/media/media_stream_options.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

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

    // Ignore internal constraints set by JS.
    // TODO(jiayl): replace the hard coded string with
    // webrtc::MediaConstraintsInterface::kInternalConstraintPrefix when
    // the Libjingle change is rolled.
    if (StartsWithASCII(new_constraint.key, "internal", true))
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

void RTCMediaConstraints::AddOptional(const std::string& key,
                                      const std::string& value) {
  optional_.push_back(Constraint(key, value));
}

}  // namespace content
