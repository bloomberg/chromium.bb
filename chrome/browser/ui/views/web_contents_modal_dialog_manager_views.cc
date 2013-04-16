// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

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
      NativeWebContentsModalDialogManagerDelegate* native_delegate)
      : native_delegate_(native_delegate) {
  }

  virtual ~NativeWebContentsModalDialogManagerViews() {
    for (std::set<views::Widget*>::iterator it = observed_widgets_.begin();
         it != observed_widgets_.end();
         ++it) {
      (*it)->RemoveObserver(this);
    }
  }

  // NativeWebContentsModalDialogManager overrides
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    views::Widget* widget = GetWidget(dialog);
    widget->AddObserver(this);
    observed_widgets_.insert(widget);
    widget->set_movement_disabled(true);

#if defined(USE_AURA)
    // TODO(wittman): remove once the new visual style is complete
    widget->GetNativeWindow()->SetProperty(aura::client::kConstrainedWindowKey,
                                           true);

    if (views::DialogDelegate::UseNewStyle()) {
      views::corewm::SetWindowVisibilityAnimationType(
          widget->GetNativeWindow(),
          views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE);
    }
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

  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    views::Widget* widget = GetWidget(dialog);
#if defined(USE_AURA)
    scoped_ptr<views::corewm::SuspendChildWindowVisibilityAnimations> suspend;
    if (views::DialogDelegate::UseNewStyle() &&
        shown_widgets_.find(widget) != shown_widgets_.end()) {
      suspend.reset(new views::corewm::SuspendChildWindowVisibilityAnimations(
          widget->GetNativeWindow()->parent()));
    }
#endif
    widget->Show();
    FocusDialog(dialog);
#if defined(USE_AURA)
    if (views::DialogDelegate::UseNewStyle()) {
      widget->GetNativeWindow()->parent()->StackChildAbove(
          widget->GetNativeWindow(),
          native_delegate_->GetWebContents()->GetView()->GetNativeView());
    }

    shown_widgets_.insert(widget);
#endif
  }

  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    views::Widget* widget = GetWidget(dialog);
#if defined(USE_AURA)
    scoped_ptr<views::corewm::SuspendChildWindowVisibilityAnimations> suspend;
    if (views::DialogDelegate::UseNewStyle()) {
      suspend.reset(new views::corewm::SuspendChildWindowVisibilityAnimations(
          widget->GetNativeWindow()->parent()));
    }
#endif
    widget->Hide();
  }

  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetWidget(dialog)->Close();
  }

  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    views::Widget* widget = GetWidget(dialog);
    if (widget->widget_delegate() &&
        widget->widget_delegate()->GetInitiallyFocusedView())
      widget->widget_delegate()->GetInitiallyFocusedView()->RequestFocus();
#if defined(USE_ASH)
    // We don't necessarily have a RootWindow yet.
    if (widget->GetNativeView()->GetRootWindow())
      widget->GetNativeView()->Focus();
#endif
  }

  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

  // views::WidgetObserver overrides

  // NOTE(wittman): OnWidgetClosing is overriden to ensure that, when the widget
  // is explicitly closed, the destruction occurs within the same call
  // stack. This avoids event races that lead to non-deterministic destruction
  // ordering in e.g. the print preview dialog. OnWidgetDestroying is overridden
  // because OnWidgetClosing is *only* invoked on explicit close, not when the
  // widget is implicitly destroyed due to its parent being closed. This
  // situation occurs with app windows.  WidgetClosing removes the observer, so
  // only one of these two functions is ever invoked for a given widget.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE {
    WidgetClosing(widget);
  }

  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    WidgetClosing(widget);
  }

 private:
  static views::Widget* GetWidget(NativeWebContentsModalDialog dialog) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(dialog);
    DCHECK(widget);
    return widget;
  }

  void WidgetClosing(views::Widget* widget) {
#if defined(USE_ASH)
    gfx::NativeView view = platform_util::GetParent(widget->GetNativeView());
    // Allow the parent to animate again.
    if (view && view->parent())
      view->parent()->ClearProperty(aura::client::kAnimationsDisabledKey);
#endif
    widget->RemoveObserver(this);
    native_delegate_->WillClose(widget->GetNativeView());
    observed_widgets_.erase(widget);
#if defined(USE_AURA)
    shown_widgets_.erase(widget);
#endif
  }

  NativeWebContentsModalDialogManagerDelegate* native_delegate_;
  std::set<views::Widget*> observed_widgets_;
  std::set<views::Widget*> shown_widgets_;

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerViews);
};

}  // namespace

NativeWebContentsModalDialogManager* WebContentsModalDialogManager::
CreateNativeManager(
    NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerViews(native_delegate);
}
