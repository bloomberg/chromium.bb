// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/native_widget_view_manager.h"

#include "mandoline/ui/aura/input_method_mandoline.h"
#include "mandoline/ui/aura/window_tree_host_mojo.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"

namespace mandoline {
namespace {

// TODO: figure out what this should be.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  FocusRulesImpl() {}
  ~FocusRulesImpl() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

}  // namespace

NativeWidgetViewManager::NativeWidgetViewManager(
    views::internal::NativeWidgetDelegate* delegate,
    mojo::Shell* shell,
    mus::View* view)
    : NativeWidgetAura(delegate), view_(view) {
  view_->AddObserver(this);
  window_tree_host_.reset(new WindowTreeHostMojo(shell, view_));
  window_tree_host_->InitHost();

  focus_client_.reset(new wm::FocusController(new FocusRulesImpl));

  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_client_.get());
  window_tree_host_->window()->AddPreTargetHandler(focus_client_.get());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));
}

NativeWidgetViewManager::~NativeWidgetViewManager() {
  if (view_)
    view_->RemoveObserver(this);
}

void NativeWidgetViewManager::InitNativeWidget(
    const views::Widget::InitParams& in_params) {
  views::Widget::InitParams params(in_params);
  params.parent = window_tree_host_->window();
  NativeWidgetAura::InitNativeWidget(params);
}

void NativeWidgetViewManager::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  view_->SetVisible(visible);
  // NOTE: We could also update aura::Window's visibility when the View's
  // visibility changes, but this code isn't going to be around for very long so
  // I'm not bothering.
}

void NativeWidgetViewManager::OnViewDestroyed(mus::View* view) {
  DCHECK_EQ(view, view_);
  view->RemoveObserver(this);
  view_ = NULL;
  // TODO(sky): WindowTreeHostMojo assumes the View outlives it.
  // NativeWidgetViewManager needs to deal, likely by deleting this.
}

void NativeWidgetViewManager::OnViewBoundsChanged(
    mus::View* view,
    const mojo::Rect& old_bounds,
    const mojo::Rect& new_bounds) {
  gfx::Rect view_rect = view->bounds().To<gfx::Rect>();
  GetWidget()->SetBounds(gfx::Rect(view_rect.size()));
}

void NativeWidgetViewManager::OnViewFocusChanged(mus::View* gained_focus,
                                                 mus::View* lost_focus) {
  if (gained_focus == view_)
    window_tree_host_->GetInputMethod()->OnFocus();
  else if (lost_focus == view_)
    window_tree_host_->GetInputMethod()->OnBlur();
}

void NativeWidgetViewManager::OnViewInputEvent(mus::View* view,
                                               const mojo::EventPtr& event) {
  scoped_ptr<ui::Event> ui_event(event.To<scoped_ptr<ui::Event>>());
  if (!ui_event)
    return;

  if (ui_event->IsKeyEvent()) {
    window_tree_host_->GetInputMethod()->DispatchKeyEvent(
        static_cast<ui::KeyEvent*>(ui_event.get()));
  } else {
    window_tree_host_->SendEventToProcessor(ui_event.get());
  }
}

}  // namespace mandoline
