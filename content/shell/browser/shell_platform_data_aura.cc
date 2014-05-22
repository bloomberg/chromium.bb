// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_data_aura.h"

#include "content/shell/browser/shell.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/default_activation_client.h"

namespace content {

namespace {

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root)
      : root_(root) {
  }

  virtual ~FillLayout() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {
  }

  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    child->SetBounds(root_->bounds());
  }

  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {
  }

  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
  }

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
  }

  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

class MinimalInputEventFilter : public ui::internal::InputMethodDelegate,
                                public ui::EventHandler {
 public:
  explicit MinimalInputEventFilter(aura::WindowTreeHost* host)
      : host_(host),
        input_method_(ui::CreateInputMethod(this,
                                            gfx::kNullAcceleratedWidget)) {
    input_method_->Init(true);
    host_->window()->AddPreTargetHandler(this);
    host_->window()->SetProperty(aura::client::kRootWindowInputMethodKey,
                                 input_method_.get());
  }

  virtual ~MinimalInputEventFilter() {
    host_->window()->RemovePreTargetHandler(this);
    host_->window()->SetProperty(aura::client::kRootWindowInputMethodKey,
                                 static_cast<ui::InputMethod*>(NULL));
  }

 private:
  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    // See the comment in InputMethodEventFilter::OnKeyEvent() for details.
    if (event->IsTranslated()) {
      event->SetTranslated(false);
    } else {
      if (input_method_->DispatchKeyEvent(*event))
        event->StopPropagation();
    }
  }

  // ui::internal::InputMethodDelegate:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) OVERRIDE {
    // See the comment in InputMethodEventFilter::DispatchKeyEventPostIME() for
    // details.
    ui::KeyEvent aura_event(event);
    aura_event.SetTranslated(true);
    ui::EventDispatchDetails details =
        host_->dispatcher()->OnEventFromSource(&aura_event);
    return aura_event.handled() || details.dispatcher_destroyed;
  }

  aura::WindowTreeHost* host_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(MinimalInputEventFilter);
};

}

ShellPlatformDataAura* Shell::platform_ = NULL;

ShellPlatformDataAura::ShellPlatformDataAura(const gfx::Size& initial_size) {
  CHECK(aura::Env::GetInstance());
  host_.reset(aura::WindowTreeHost::Create(gfx::Rect(initial_size)));
  host_->InitHost();
  host_->window()->SetLayoutManager(new FillLayout(host_->window()));

  focus_client_.reset(new aura::test::TestFocusClient());
  aura::client::SetFocusClient(host_->window(), focus_client_.get());

  new wm::DefaultActivationClient(host_->window());
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));
  window_tree_client_.reset(
      new aura::test::TestWindowTreeClient(host_->window()));
  ime_filter_.reset(new MinimalInputEventFilter(host_.get()));
}

ShellPlatformDataAura::~ShellPlatformDataAura() {
}

void ShellPlatformDataAura::ShowWindow() {
  host_->Show();
}

void ShellPlatformDataAura::ResizeWindow(const gfx::Size& size) {
  host_->SetBounds(gfx::Rect(size));
}

}  // namespace content
