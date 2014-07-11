// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/examples/keyboard/keyboard.mojom.h"
#include "mojo/examples/keyboard/keyboard_delegate.h"
#include "mojo/examples/keyboard/keyboard_view.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "ui/events/event.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

using mojo::view_manager::Id;

namespace mojo {
namespace examples {

class Keyboard;

class KeyboardServiceImpl : public InterfaceImpl<KeyboardService> {
 public:
  KeyboardServiceImpl(ApplicationConnection* connection, Keyboard* keyboard);
  virtual ~KeyboardServiceImpl() {}

  // KeyboardService:
  virtual void SetTarget(uint32_t node_id) OVERRIDE;

 private:
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardServiceImpl);
};

class Keyboard : public ApplicationDelegate,
                 public view_manager::ViewManagerDelegate,
                 public KeyboardDelegate {
 public:
  Keyboard() : view_manager_(NULL), keyboard_service_(NULL), target_(0) {}

  virtual ~Keyboard() {
  }

  void set_target(Id id) { target_ = id; }

  void set_keyboard_service(KeyboardServiceImpl* keyboard) {
    keyboard_service_ = keyboard;
  }

 private:
  // Overridden from ApplicationDelegate:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    views_init_.reset(new ViewsInit);
    view_manager::ViewManager::ConfigureIncomingConnection(connection, this);
    connection->AddService<KeyboardServiceImpl>(this);
    return true;
  }

  void CreateWidget(view_manager::Node* node) {
    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->AddChildView(new KeyboardView(this));
    widget_delegate->GetContentsView()->SetLayoutManager(new views::FillLayout);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.native_widget = new NativeWidgetViewManager(widget, node);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(node->bounds().width(), node->bounds().height());
    widget->Init(params);
    widget->Show();
  }

  // view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    // TODO: deal with OnRootAdded() being invoked multiple times.
    view_manager_ = view_manager;
    root->SetActiveView(view_manager::View::Create(view_manager));
    CreateWidget(root);
  }
  virtual void OnViewManagerDisconnected(
      view_manager::ViewManager* view_manager) OVERRIDE {
    DCHECK_EQ(view_manager_, view_manager);
    view_manager_ = NULL;
    base::MessageLoop::current()->Quit();
  }

  // KeyboardDelegate:
  virtual void OnKeyPressed(int key_code, int event_flags) OVERRIDE {
    if (!target_)
      return;
    keyboard_service_->client()->OnKeyboardEvent(target_, key_code,
                                                 event_flags);
  }

  scoped_ptr<ViewsInit> views_init_;

  view_manager::ViewManager* view_manager_;

  KeyboardServiceImpl* keyboard_service_;

  Id target_;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

KeyboardServiceImpl::KeyboardServiceImpl(ApplicationConnection* connection,
                                         Keyboard* keyboard)
    : keyboard_(keyboard) {
  keyboard_->set_keyboard_service(this);
}

void KeyboardServiceImpl::SetTarget(uint32_t node_id) {
  keyboard_->set_target(node_id);
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::Keyboard;
}

}  // namespace mojo
