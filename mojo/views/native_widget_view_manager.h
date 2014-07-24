// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_VIEWS_NATIVE_WIDGET_VIEW_MANAGER_H_
#define MOJO_VIEWS_NATIVE_WIDGET_VIEW_MANAGER_H_

#include "mojo/aura/window_tree_host_mojo_delegate.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "ui/views/widget/native_widget_aura.h"

namespace ui {
namespace internal {
class InputMethodDelegate;
}
}

namespace wm {
class FocusController;
class ScopedCaptureClient;
}

namespace mojo {

class WindowTreeHostMojo;

class NativeWidgetViewManager : public views::NativeWidgetAura,
                                public WindowTreeHostMojoDelegate,
                                public ViewObserver,
                                public NodeObserver {
 public:
  NativeWidgetViewManager(views::internal::NativeWidgetDelegate* delegate,
                          Node* node);
  virtual ~NativeWidgetViewManager();

 private:
  // Overridden from internal::NativeWidgetAura:
  virtual void InitNativeWidget(
      const views::Widget::InitParams& in_params) OVERRIDE;

  // WindowTreeHostMojoDelegate:
  virtual void CompositorContentsChanged(const SkBitmap& bitmap) OVERRIDE;

  // NodeObserver:
  virtual void OnNodeDestroyed(Node* node) OVERRIDE;
  virtual void OnNodeActiveViewChanged(Node* node,
                                       View* old_view,
                                       View* new_view) OVERRIDE;
  virtual void OnNodeBoundsChanged(Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE;

  // ViewObserver
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE;
  virtual void OnViewDestroyed(View* view) OVERRIDE;

  scoped_ptr<WindowTreeHostMojo> window_tree_host_;

  scoped_ptr<wm::FocusController> focus_client_;

  scoped_ptr<ui::internal::InputMethodDelegate> ime_filter_;

  Node* node_;
  View* view_;

  scoped_ptr<wm::ScopedCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetViewManager);
};

}  // namespace mojo

#endif  // MOJO_VIEWS_NATIVE_WIDGET_VIEW_MANAGER_H_
