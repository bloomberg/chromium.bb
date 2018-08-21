// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/render_loop_factory.h"

#include <utility>

#include "chrome/browser/android/vr/gvr_controller_delegate.h"
#include "chrome/browser/android/vr/gvr_keyboard_delegate.h"
#include "chrome/browser/android/vr/vr_gl_thread.h"
#include "chrome/browser/android/vr/vr_shell_gl.h"
#include "chrome/browser/vr/sounds_manager_audio_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui_factory.h"

namespace vr {

RenderLoopFactory::Params::Params(
    gvr::GvrApi* gvr_api,
    const UiInitialState& ui_initial_state,
    bool reprojected_rendering,
    bool daydream_support,
    bool pause_content,
    bool low_density,
    base::WaitableEvent* gl_surface_created_event,
    base::OnceCallback<gfx::AcceleratedWidget()> surface_callback)
    : gvr_api(gvr_api),
      ui_initial_state(ui_initial_state),
      reprojected_rendering(reprojected_rendering),
      daydream_support(daydream_support),
      pause_content(pause_content),
      low_density(low_density),
      gl_surface_created_event(gl_surface_created_event),
      surface_callback(std::move(surface_callback)) {}

RenderLoopFactory::Params::~Params() = default;

std::unique_ptr<VrShellGl> RenderLoopFactory::Create(
    VrGLThread* vr_gl_thread,
    UiFactory* ui_factory,
    std::unique_ptr<Params> params) {
  DCHECK(params);
  auto keyboard_delegate = GvrKeyboardDelegate::Create();
  auto text_input_delegate = std::make_unique<TextInputDelegate>();
  if (!keyboard_delegate) {
    params->ui_initial_state.needs_keyboard_update = true;
  } else {
    text_input_delegate->SetUpdateInputCallback(
        base::BindRepeating(&KeyboardDelegate::UpdateInput,
                            base::Unretained(keyboard_delegate.get())));
  }
  auto audio_delegate = std::make_unique<SoundsManagerAudioDelegate>();
  auto ui = ui_factory->Create(
      vr_gl_thread, vr_gl_thread, std::move(keyboard_delegate),
      std::move(text_input_delegate), std::move(audio_delegate),
      params->ui_initial_state);
  auto controller_delegate =
      std::make_unique<GvrControllerDelegate>(params->gvr_api, vr_gl_thread);
  auto vr_shell_gl = std::make_unique<VrShellGl>(
      vr_gl_thread, std::move(ui), std::move(controller_delegate),
      params->gvr_api, params->reprojected_rendering, params->daydream_support,
      params->ui_initial_state.in_web_vr, params->pause_content,
      params->low_density);
  vr_gl_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShellGl::Init, vr_shell_gl->GetWeakPtr(),
                     base::Unretained(params->gl_surface_created_event),
                     base::Passed(std::move(params->surface_callback))));
  return vr_shell_gl;
}

}  // namespace vr
