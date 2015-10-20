// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/android/webmediasession_android.h"

namespace content {

void WebMediaSessionAndroid::activate(
    blink::WebMediaSessionActivateCallback* raw_callback) {
  NOTIMPLEMENTED();

  scoped_ptr<blink::WebMediaSessionActivateCallback> callback(raw_callback);
  callback->onError(blink::WebMediaSessionError::Activate);
}

void WebMediaSessionAndroid::deactivate(
    blink::WebMediaSessionDeactivateCallback* raw_callback) {
  NOTIMPLEMENTED();

  scoped_ptr<blink::WebMediaSessionDeactivateCallback> callback(raw_callback);
  callback->onSuccess();
}

}  // namespace content
