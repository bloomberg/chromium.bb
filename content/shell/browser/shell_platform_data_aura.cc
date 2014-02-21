// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_data_aura.h"

#include "content/shell/browser/shell.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_activation_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/gfx/screen.h"

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
  explicit MinimalInputEventFilter(aura::WindowEventDispatcher* dispatcher)
      : dispatcher_(dispatcher),
        input_method_(ui::CreateInputMethod(this,
                                            gfx::kNullAcceleratedWidget)) {
    input_method_->Init(true);
    dispatcher_->window()->AddPreTargetHandler(this);
    dispatcher_->window()->SetProperty(aura::client::kRootWindowInputMethodKey,
                                       input_method_.get());
  }

  virtual ~MinimalInputEventFilter() {
    dispatcher_->window()->RemovePreTargetHandler(this);
    dispatcher_->window()->SetProperty(aura::client::kRootWindowInputMethodKey,
                                       static_cast<ui::InputMethod*>(NULL));
  }

 private:
  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    const ui::EventType type = event->type();
    if (type == ui::ET_TRANSLATED_KEY_PRESS ||
        type == ui::ET_TRANSLATED_KEY_RELEASE) {
      // The |event| is already handled by this object, change the type of the
      // event to ui::ET_KEY_* and pass it to the next filter.
      static_cast<ui::TranslatedKeyEvent*>(event)->ConvertToKeyEvent();
    } else {
      if (input_method_->DispatchKeyEvent(*event))
        event->StopPropagation();
    }
  }

  // ui::internal::InputMethodDelegate:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) OVERRIDE {
    ui::TranslatedKeyEvent aura_event(event);
    ui::EventDispatchDetails details =
        dispatcher_->OnEventFromSource(&aura_event);
    return aura_event.handled() || details.dispatcher_destroyed;
  }

  aura::WindowEventDispatcher* dispatcher_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(MinimalInputEventFilter);
};

}

ShellPlatformDataAura* Shell::platform_ = NULL;

ShellPlatformDataAura::ShellPlatformDataAura(const gfx::Size& initial_size) {
  aura::Env::CreateInstance();
  dispatcher_.reset(new aura::WindowEventDispatcher(
      aura::WindowEventDispatcher::CreateParams(gfx::Rect(initial_size))));
  dispatcher_->host()->InitHost();
  dispatcher_->window()->SetLayoutManager(
      new FillLayout(dispatcher_->window()));

  focus_client_.reset(new aura::test::TestFocusClient());
  aura::client::SetFocusClient(dispatcher_->window(), focus_client_.get());

  activation_client_.reset(
      new aura::client::DefaultActivationClient(dispatcher_->window()));
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(dispatcher_->window()));
  window_tree_client_.reset(
      new aura::test::TestWindowTreeClient(dispatcher_->window()));
  ime_filter_.reset(new MinimalInputEventFilter(dispatcher_.get()));
}

ShellPlatformDataAura::~ShellPlatformDataAura() {
}

void ShellPlatformDataAura::ShowWindow() {
  dispatcher_->host()->Show();
}

void ShellPlatformDataAura::ResizeWindow(const gfx::Size& size) {
  dispatcher_->host()->SetBounds(gfx::Rect(size));
}



}  // namespace content
