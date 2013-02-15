// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_MEDIA_CONSTRAINTS_H_
#define CONTENT_RENDERER_MEDIA_RTC_MEDIA_CONSTRAINTS_H_

#include "base/compiler_specific.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace WebKit {
class WebMediaConstraints;
}

namespace content {

// RTCMediaConstraints acts as a glue layer between WebKits MediaConstraints and
// libjingle webrtc::MediaConstraintsInterface.
// Constraints are used by PeerConnection and getUserMedia API calls.
class RTCMediaConstraints : public webrtc::MediaConstraintsInterface {
 public:
  explicit RTCMediaConstraints(
      const WebKit::WebMediaConstraints& constraints);
  virtual ~RTCMediaConstraints();
  virtual const Constraints& GetMandatory() const OVERRIDE;
  virtual const Constraints& GetOptional() const OVERRIDE;

 protected:
  Constraints mandatory_;
  Constraints optional_;
};

}  // namespace content


#endif  // CONTENT_RENDERER_MEDIA_RTC_MEDIA_CONSTRAINTS_H_
