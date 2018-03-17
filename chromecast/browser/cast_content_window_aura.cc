// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_aura.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace chromecast {
namespace shell {

class TouchBlocker : public ui::EventHandler, public aura::WindowObserver {
 public:
  explicit TouchBlocker(aura::Window* window) : window_(window) {
    DCHECK(window_);
    window_->AddObserver(this);
    window_->AddPreTargetHandler(this);
  }

  ~TouchBlocker() override {
    if (window_) {
      window_->RemoveObserver(this);
      window_->RemovePreTargetHandler(this);
    }
  }

 private:
  // Overriden from ui::EventHandler.
  void OnTouchEvent(ui::TouchEvent* touch) override { touch->SetHandled(); }

  // Overriden from aura::WindowObserver.
  void OnWindowDestroyed(aura::Window* window) override { window_ = nullptr; }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(TouchBlocker);
};

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate,
    bool is_headless,
    bool enable_touch_input) {
  return base::WrapUnique(new CastContentWindowAura(enable_touch_input));
}

CastContentWindowAura::~CastContentWindowAura() = default;

CastContentWindowAura::CastContentWindowAura(bool is_touch_enabled)
    : is_touch_enabled_(is_touch_enabled), touch_blocker_() {}

void CastContentWindowAura::CreateWindowForWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager,
    bool is_visible,
    VisibilityPriority visibility_priority) {
  DCHECK(web_contents);
  DCHECK(window_manager);
  gfx::NativeView window = web_contents->GetNativeView();
  window_manager->SetWindowId(window, CastWindowManager::APP);
  window_manager->AddWindow(window);

  if (!is_touch_enabled_) {
    touch_blocker_ = std::make_unique<TouchBlocker>(window);
  }

  if (is_visible) {
    window->Show();
  } else {
    window->Hide();
  }
}

void CastContentWindowAura::EnableTouchInput(bool enabled) {
  // TODO(halliwell): implement this
}

void CastContentWindowAura::RequestVisibility(
    VisibilityPriority visibility_priority){};

void CastContentWindowAura::RequestMoveOut(){};

}  // namespace shell
}  // namespace chromecast
