// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIASESSION_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIASESSION_ANDROID_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/mediasession/WebMediaSession.h"

namespace content {

class WebMediaSessionAndroid : public blink::WebMediaSession {
 public:
  WebMediaSessionAndroid() = default;
  virtual ~WebMediaSessionAndroid() = default;

  void activate(blink::WebMediaSessionActivateCallback*) override;
  void deactivate(blink::WebMediaSessionDeactivateCallback*) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaSessionAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIASESSION_ANDROID_H_
