// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_H_
#define CHROME_BROWSER_VR_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/ui_interface.h"

namespace vr {
class BrowserUiInterface;
class ContentInputDelegate;
class UiBrowserInterface;
class UiInputManager;
class UiRenderer;
class UiScene;
class UiSceneManager;
class VrShellRenderer;
}  // namespace vr

namespace vr {

struct UiInitialState {
  bool in_cct = false;
  bool in_web_vr = false;
  bool web_vr_autopresentation_expected = false;
};

// This class manages all GLThread owned objects and GL rendering for VrShell.
// It is not threadsafe and must only be used on the GL thread.
class Ui {
 public:
  Ui(UiBrowserInterface* browser,
     ContentInputDelegate* content_input_delegate,
     const vr::UiInitialState& ui_initial_state);
  virtual ~Ui();

  base::WeakPtr<vr::BrowserUiInterface> GetBrowserUiWeakPtr();

  void OnGlInitialized(unsigned int content_texture_id);

  UiInterface* ui();

  // TODO(crbug.com/767957): Refactor to hide these behind the UI interface.
  UiScene* scene() { return scene_.get(); }
  VrShellRenderer* vr_shell_renderer() { return vr_shell_renderer_.get(); }
  UiRenderer* ui_renderer() { return ui_renderer_.get(); }
  UiInputManager* input_manager() { return input_manager_.get(); }

 private:
  // This state may be further abstracted into a SkiaUi object.
  std::unique_ptr<vr::UiScene> scene_;
  std::unique_ptr<vr::UiSceneManager> scene_manager_;
  std::unique_ptr<vr::VrShellRenderer> vr_shell_renderer_;
  std::unique_ptr<vr::UiInputManager> input_manager_;
  std::unique_ptr<vr::UiRenderer> ui_renderer_;

  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_H_
