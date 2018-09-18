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
  TouchBlocker(aura::Window* window, bool activated)
      : window_(window), activated_(activated) {
    DCHECK(window_);
    window_->AddObserver(this);
    if (activated_) {
      window_->AddPreTargetHandler(this);
    }
  }

  ~TouchBlocker() override {
    if (window_) {
      window_->RemoveObserver(this);
      if (activated_) {
        window_->RemovePreTargetHandler(this);
      }
    }
  }

  void Activate(bool activate) {
    if (!window_ || activate == activated_) {
      return;
    }

    if (activate) {
      window_->AddPreTargetHandler(this);
    } else {
      window_->RemovePreTargetHandler(this);
    }

    activated_ = activate;
  }

 private:
  // Overriden from ui::EventHandler.
  void OnTouchEvent(ui::TouchEvent* touch) override {
    if (activated_) {
      touch->SetHandled();
    }
  }

  // Overriden from aura::WindowObserver.
  void OnWindowDestroyed(aura::Window* window) override { window_ = nullptr; }

  aura::Window* window_;
  bool activated_;

  DISALLOW_COPY_AND_ASSIGN(TouchBlocker);
};

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    const CastContentWindow::CreateParams& params) {
  return base::WrapUnique(new CastContentWindowAura(params));
}

CastContentWindowAura::CastContentWindowAura(
    const CastContentWindow::CreateParams& params)
    : delegate_(params.delegate),
      gesture_dispatcher_(
          std::make_unique<CastContentGestureHandler>(delegate_)),
      gesture_priority_(params.gesture_priority),
      is_touch_enabled_(params.enable_touch_input),
      window_(nullptr),
      has_screen_access_(false) {
  DCHECK(delegate_);
}

CastContentWindowAura::~CastContentWindowAura() {
  if (window_manager_) {
    window_manager_->RemoveGestureHandler(gesture_dispatcher_.get());
  }
  if (window_) {
    window_->RemoveObserver(this);
  }
}

void CastContentWindowAura::CreateWindowForWebContents(
    content::WebContents* web_contents,
    CastWindowManager* window_manager,
    CastWindowManager::WindowId z_order,
    VisibilityPriority visibility_priority) {
  DCHECK(web_contents);
  window_manager_ = window_manager;
  DCHECK(window_manager_);
  window_ = web_contents->GetNativeView();
  if (!window_->HasObserver(this)) {
    window_->AddObserver(this);
  }
  window_manager_->SetWindowId(window_, z_order);
  window_manager_->AddWindow(window_);
  window_manager_->AddGestureHandler(gesture_dispatcher_.get());

  touch_blocker_ = std::make_unique<TouchBlocker>(window_, !is_touch_enabled_);

  if (has_screen_access_) {
    window_->Show();
  } else {
    window_->Hide();
  }
}

void CastContentWindowAura::GrantScreenAccess() {
  has_screen_access_ = true;
  if (window_) {
    window_->Show();
  }
}

void CastContentWindowAura::RevokeScreenAccess() {
  has_screen_access_ = false;
  if (window_) {
    window_->Hide();
  }
}

void CastContentWindowAura::EnableTouchInput(bool enabled) {
  if (touch_blocker_) {
    touch_blocker_->Activate(!enabled);
  }
}

void CastContentWindowAura::RequestVisibility(
    VisibilityPriority visibility_priority){};

void CastContentWindowAura::NotifyVisibilityChange(
    VisibilityType visibility_type) {
  delegate_->OnVisibilityChange(visibility_type);
  for (auto& observer : observer_list_) {
    observer.OnVisibilityChange(visibility_type);
  }
}

void CastContentWindowAura::RequestMoveOut(){};

void CastContentWindowAura::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (visible) {
    gesture_dispatcher_->SetPriority(gesture_priority_);
  } else {
    gesture_dispatcher_->SetPriority(CastGestureHandler::Priority::NONE);
  }
}

void CastContentWindowAura::OnWindowDestroyed(aura::Window* window) {
  window_ = nullptr;
}

}  // namespace shell
}  // namespace chromecast
