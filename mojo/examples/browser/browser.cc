// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace mojo {
namespace examples {

// This is the basics of creating a views widget with a textfield.
// TODO: cleanup!
class Browser : public Application, public view_manager::ViewManagerDelegate {
 public:
  Browser() : view_manager_(NULL), view_(NULL) {}

  virtual ~Browser() {
  }

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    views_init_.reset(new ViewsInit);

    view_manager::ViewManager::Create(this, this);
  }

  void CreateWidget() {
    views::Textfield* textfield = new views::Textfield;

    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->AddChildView(textfield);
    widget_delegate->GetContentsView()->SetLayoutManager(new views::FillLayout);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.native_widget = new NativeWidgetViewManager(widget, view_);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(200, 200);
    widget->Init(params);
    widget->Show();
    textfield->RequestFocus();
  }

  // view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::ViewTreeNode* root) OVERRIDE {
    // TODO: deal with OnRootAdded() being invoked multiple times.
    view_manager_ = view_manager;
    view_ = view_manager::View::Create(view_manager_);
    view_manager_->GetRoots().front()->SetActiveView(view_);

    CreateWidget();
  }
  virtual void OnRootRemoved(view_manager::ViewManager* view_manager,
                             view_manager::ViewTreeNode* root) OVERRIDE {
  }

  scoped_ptr<ViewsInit> views_init_;

  view_manager::ViewManager* view_manager_;
  view_manager::View* view_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::Browser;
}

}  // namespace mojo
