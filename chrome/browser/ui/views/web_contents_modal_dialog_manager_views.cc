// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/corewm/window_modality_controller.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_constants.h"
#include "ash/shell.h"
#include "ash/wm/custom_frame_view_ash.h"
#endif

namespace {

class NativeWebContentsModalDialogManagerViews
    : public NativeWebContentsModalDialogManager,
      public views::WidgetObserver {
 public:
  NativeWebContentsModalDialogManagerViews(
      WebContentsModalDialogManager* manager)
      : manager_(manager) {
  }

  virtual ~NativeWebContentsModalDialogManagerViews() {
    for (std::set<views::Widget*>::iterator it = observed_widgets_.begin();
         it != observed_widgets_.end();
         ++it) {
      (*it)->RemoveObserver(this);
    }
  }

  // NativeWebContentsModalDialogManager overrides
  virtual void ManageDialog(gfx::NativeWindow window) OVERRIDE {
    views::Widget* widget = GetWidget(window);
    widget->AddObserver(this);
    observed_widgets_.insert(widget);
    widget->set_movement_disabled(true);

#if defined(USE_AURA)
    // TODO(wittman): remove once the new visual style is complete
    widget->GetNativeWindow()->SetProperty(aura::client::kConstrainedWindowKey,
                                           true);
#endif

#if defined(USE_ASH)
    gfx::NativeView parent = platform_util::GetParent(widget->GetNativeView());
    views::corewm::SetChildWindowVisibilityChangesAnimated(parent);
    // No animations should get performed on the window since that will re-order
    // the window stack which will then cause many problems.
    if (parent && parent->parent()) {
      parent->parent()->SetProperty(aura::client::kAnimationsDisabledKey, true);
    }

    // TODO(wittman): remove once the new visual style is complete
    widget->GetNativeWindow()->SetProperty(ash::kConstrainedWindowKey, true);
    views::corewm::SetModalParent(
        widget->GetNativeWindow(),
        platform_util::GetParent(widget->GetNativeView()));
#endif
  }

  virtual void CloseDialog(gfx::NativeWindow window) OVERRIDE {
    views::Widget* widget = GetWidget(window);
#if defined(USE_ASH)
    gfx::NativeView view = platform_util::GetParent(widget->GetNativeView());
    // Allow the parent to animate again.
    if (view && view->parent())
      view->parent()->ClearProperty(aura::client::kAnimationsDisabledKey);
#endif
    widget->Close();
  }

  // views::WidgetObserver overrides
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE {
    manager_->WillClose(static_cast<ConstrainedWindowViews*>(widget));
    observed_widgets_.erase(widget);
  }

 private:
  static views::Widget* GetWidget(gfx::NativeWindow window) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    DCHECK(widget);
    return widget;
  }

  WebContentsModalDialogManager* manager_;
  std::set<views::Widget*> observed_widgets_;

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerViews);
};

}  // namespace

NativeWebContentsModalDialogManager* WebContentsModalDialogManager::
CreateNativeManager(WebContentsModalDialogManager* manager) {
  return new NativeWebContentsModalDialogManagerViews(manager);
}
