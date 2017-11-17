// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_MODEL_H_

#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/model/modal_prompt_type.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/model/speech_recognition_model.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/model/web_vr_timeout_state.h"

namespace vr {

struct Model {
  Model();
  ~Model();

  bool loading = false;
  float load_progress = 0.0f;

  WebVrTimeoutState web_vr_timeout_state = kWebVrNoTimeoutPending;
  bool started_for_autopresentation = false;

  SpeechRecognitionModel speech;
  ControllerModel controller;
  ReticleModel reticle;

  bool experimental_features_enabled = false;
  bool incognito = false;

  ModalPromptType active_modal_prompt_type = kModalPromptTypeNone;

  ToolbarState toolbar_state;
  std::vector<OmniboxSuggestion> omnibox_suggestions;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_MODEL_H_
