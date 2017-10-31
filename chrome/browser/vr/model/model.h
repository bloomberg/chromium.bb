// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_MODEL_H_

#include "chrome/browser/vr/model/omnibox_suggestions.h"

namespace vr {

// As we wait for WebVR frames, we may pass through the following states.
enum WebVrTimeoutState {
  // We are not awaiting a WebVR frame.
  kWebVrNoTimeoutPending,
  kWebVrAwaitingFirstFrame,
  // We are awaiting a WebVR frame, and we will soon exceed the amount of time
  // that we're willing to wait. In this state, it could be appropriate to show
  // an affordance to the user to let them know that WebVR is delayed (eg, this
  // would be when we might show a spinner or progress bar).
  kWebVrTimeoutImminent,
  // In this case the time allotted for waiting for the first WebVR frame has
  // been entirely exceeded. This would, for example, be an appropriate time to
  // show "sad tab" UI to allow the user to bail on the WebVR content.
  kWebVrTimedOut,
};

struct Model {
  Model();
  ~Model();

  bool loading = false;
  float load_progress = 0.0f;

  WebVrTimeoutState web_vr_timeout_state = kWebVrNoTimeoutPending;
  bool started_for_autopresentation = false;

  bool recognizing_speech = false;
  int speech_recognition_state = 0;

  std::vector<OmniboxSuggestion> omnibox_suggestions;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_MODEL_H_
