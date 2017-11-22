// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_MODEL_H_

#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/model/modal_prompt_type.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/permissions_model.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/model/speech_recognition_model.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/model/web_vr_timeout_state.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/transform.h"

namespace vr {

struct Model {
  Model();
  ~Model();

  // VR browsing state.
  bool browsing_disabled = false;
  bool loading = false;
  float load_progress = 0.0f;
  bool fullscreen = false;
  bool incognito = false;
  bool in_cct = false;
  bool can_navigate_back = false;
  ToolbarState toolbar_state;
  std::vector<OmniboxSuggestion> omnibox_suggestions;
  bool omnibox_input_active = false;
  SpeechRecognitionModel speech;
  const ColorScheme& color_scheme() const;
  gfx::Transform projection_matrix;
  unsigned int content_texture_id = 0;
  UiElementRenderer::TextureLocation content_location =
      UiElementRenderer::kTextureLocationLocal;

  // WebVR state.
  bool web_vr_mode = false;
  bool web_vr_show_toast = false;
  bool web_vr_show_splash_screen = false;
  // Indicates that we're waiting for the first WebVR frame to show up before we
  // hide the splash screen. This is used in the case of WebVR auto-
  // presentation.
  bool web_vr_started_for_autopresentation = false;
  bool should_render_web_vr() const {
    return web_vr_mode && !web_vr_show_splash_screen;
  }
  bool browsing_mode() const {
    return !web_vr_mode && !web_vr_show_splash_screen;
  }
  WebVrTimeoutState web_vr_timeout_state = kWebVrNoTimeoutPending;

  // Controller state.
  ControllerModel controller;
  ReticleModel reticle;

  // State affecting both VR browsing and WebVR.
  ModalPromptType active_modal_prompt_type = kModalPromptTypeNone;
  PermissionsModel permissions;
  bool experimental_features_enabled = false;
  bool exiting_vr = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_MODEL_H_
