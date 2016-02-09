// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/platform_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_modality_controller.h"
#endif

using web_modal::SingleWebContentsDialogManager;
using web_modal::SingleWebContentsDialogManagerDelegate;
using web_modal::WebContentsModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace {

class NativeWebContentsModalDialogManagerViews
    : public SingleWebContentsDialogManager,
      public ModalDialogHostObserver,
      public views::WidgetObserver {
 public:
  NativeWebContentsModalDialogManagerViews(
      gfx::NativeWindow dialog,
      SingleWebContentsDialogManagerDelegate* native_delegate)
      : native_delegate_(native_delegate),
        dialog_(dialog),
        host_(NULL),
        host_destroying_(false) {
    ManageDialog();
  }

  ~NativeWebContentsModalDialogManagerViews() override {
    if (host_)
      host_->RemoveObserver(this);

    for (std::set<views::Widget*>::iterator it = observed_widgets_.begin();
         it != observed_widgets_.end();
         ++it) {
      (*it)->RemoveObserver(this);
    }
  }

  // Sets up this object to manage the dialog_. Registers for closing events
  // in order to notify the delegate.
  virtual void ManageDialog() {
    views::Widget* widget = GetWidget(dialog());
    widget->AddObserver(this);
    observed_widgets_.insert(widget);
    widget->set_movement_disabled(true);

#if defined(USE_AURA)
    // TODO(wittman): remove once the new visual style is complete
    widget->GetNativeWindow()->SetProperty(aura::client::kConstrainedWindowKey,
                                           true);

    wm::SetWindowVisibilityAnimationType(
        widget->GetNativeWindow(),
        wm::WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE);

    gfx::NativeView parent = platform_util::GetParent(widget->GetNativeView());
    wm::SetChildWindowVisibilityChangesAnimated(parent);
    // No animations should get performed on the window since that will re-order
    // the window stack which will then cause many problems.
    if (parent && parent->parent()) {
      parent->parent()->SetProperty(aura::client::kAnimationsDisabledKey, true);
    }

    wm::SetModalParent(
        widget->GetNativeWindow(),
        native_delegate_->GetWebContents()->GetNativeView());
#endif
  }

  // SingleWebContentsDialogManager overrides
  void Show() override {
    // The host destroying means the dialogs will be destroyed in short order.
    // Avoid showing dialogs at this point as the necessary native window
    // services may not be present.
    if (host_destroying_)
      return;

    views::Widget* widget = GetWidget(dialog());
#if defined(USE_AURA)
    scoped_ptr<wm::SuspendChildWindowVisibilityAnimations> suspend;
    if (shown_widgets_.find(widget) != shown_widgets_.end()) {
      suspend.reset(new wm::SuspendChildWindowVisibilityAnimations(
          widget->GetNativeWindow()->parent()));
    }
#endif
    // Host may be NULL during tab drag on Views/Win32.
    if (host_)
      constrained_window::UpdateWebContentsModalDialogPosition(widget, host_);
    widget->Show();
    Focus();

#if defined(USE_AURA)
    // TODO(pkotwicz): Control the z-order of the constrained dialog via
    // views::kHostViewKey. We will need to ensure that the parent window's
    // shadows are below the constrained dialog in z-order when we do this.
    shown_widgets_.insert(widget);
#endif
  }

  void Hide() override {
    views::Widget* widget = GetWidget(dialog());
#if defined(USE_AURA)
    scoped_ptr<wm::SuspendChildWindowVisibilityAnimations> suspend;
    suspend.reset(new wm::SuspendChildWindowVisibilityAnimations(
        widget->GetNativeWindow()->parent()));
#endif
    widget->Hide();
  }

  void Close() override { GetWidget(dialog())->Close(); }

  void Focus() override {
    views::Widget* widget = GetWidget(dialog());
    if (widget->widget_delegate() &&
        widget->widget_delegate()->GetInitiallyFocusedView())
      widget->widget_delegate()->GetInitiallyFocusedView()->RequestFocus();
#if defined(USE_AURA)
    // We don't necessarily have a RootWindow yet.
    if (widget->GetNativeView()->GetRootWindow())
      widget->GetNativeView()->Focus();
#endif
  }

  void Pulse() override {}

  // WebContentsModalDialogHostObserver overrides
  void OnPositionRequiresUpdate() override {
    DCHECK(host_);

    for (std::set<views::Widget*>::iterator it = observed_widgets_.begin();
         it != observed_widgets_.end();
         ++it) {
      constrained_window::UpdateWebContentsModalDialogPosition(*it, host_);
    }
  }

  void OnHostDestroying() override {
    host_->RemoveObserver(this);
    host_ = NULL;
    host_destroying_ = true;
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
  void OnWidgetClosing(views::Widget* widget) override {
    WidgetClosing(widget);
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    WidgetClosing(widget);
  }

  void HostChanged(web_modal::WebContentsModalDialogHost* new_host) override {
    if (host_)
      host_->RemoveObserver(this);

    host_ = new_host;

    // |host_| may be null during WebContents destruction or Win32 tab dragging.
    if (host_) {
      host_->AddObserver(this);

      for (std::set<views::Widget*>::iterator it = observed_widgets_.begin();
           it != observed_widgets_.end();
           ++it) {
        views::Widget::ReparentNativeView((*it)->GetNativeView(),
                                          host_->GetHostView());
      }

      OnPositionRequiresUpdate();
    }
  }

  gfx::NativeWindow dialog() override { return dialog_; }

 private:
  static views::Widget* GetWidget(gfx::NativeWindow dialog) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(dialog);
    DCHECK(widget);
    return widget;
  }

  void WidgetClosing(views::Widget* widget) {
#if defined(USE_AURA)
    gfx::NativeView view = platform_util::GetParent(widget->GetNativeView());
    // Allow the parent to animate again.
    if (view && view->parent())
      view->parent()->ClearProperty(aura::client::kAnimationsDisabledKey);
#endif
    widget->RemoveObserver(this);
    observed_widgets_.erase(widget);

#if defined(USE_AURA)
    shown_widgets_.erase(widget);
#endif

    // Will cause this object to be deleted.
    native_delegate_->WillClose(widget->GetNativeWindow());
  }

  SingleWebContentsDialogManagerDelegate* native_delegate_;
  gfx::NativeWindow dialog_;
  WebContentsModalDialogHost* host_;
  bool host_destroying_;
  std::set<views::Widget*> observed_widgets_;
  std::set<views::Widget*> shown_widgets_;

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerViews);
};

}  // namespace

namespace web_modal {

SingleWebContentsDialogManager*
WebContentsModalDialogManager::CreateNativeWebModalManager(
    gfx::NativeWindow dialog,
    SingleWebContentsDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerViews(dialog, native_delegate);
}

}  // namespace web_modal
