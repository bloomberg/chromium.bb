// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/examples/keyboard/keyboard.mojom.h"
#include "mojo/examples/keyboard/keyboard_delegate.h"
#include "mojo/examples/keyboard/keyboard_view.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "ui/events/event.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace mojo {
namespace examples {

class Keyboard;

class KeyboardServiceImpl : public InterfaceImpl<KeyboardService> {
 public:
  explicit KeyboardServiceImpl(Keyboard* keyboard);
  virtual ~KeyboardServiceImpl() {}

  // KeyboardService:
  virtual void SetTarget(uint32_t view_id) OVERRIDE;

 private:
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardServiceImpl);
};

class Keyboard : public ApplicationDelegate,
                 public ViewManagerDelegate,
                 public KeyboardDelegate {
 public:
  Keyboard()
      : keyboard_service_factory_(this),
        view_manager_(NULL),
        keyboard_service_(NULL),
        target_(0) {}

  virtual ~Keyboard() {
  }

  void set_target(Id id) { target_ = id; }

  void set_keyboard_service(KeyboardServiceImpl* keyboard) {
    keyboard_service_ = keyboard;
  }

 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    views_init_.reset(new ViewsInit);
    connection->AddService(view_manager_client_factory_.get());
    connection->AddService(&keyboard_service_factory_);
    return true;
  }

  void CreateWidget(View* view) {
    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->AddChildView(new KeyboardView(this));
    widget_delegate->GetContentsView()->SetLayoutManager(new views::FillLayout);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.native_widget = new NativeWidgetViewManager(widget, view);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(view->bounds().width(), view->bounds().height());
    widget->Init(params);
    widget->Show();
  }

  // ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    // TODO: deal with OnEmbed() being invoked multiple times.
    view_manager_ = view_manager;
    CreateWidget(root);
  }
  virtual void OnViewManagerDisconnected(
      ViewManager* view_manager) OVERRIDE {
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

  InterfaceFactoryImplWithContext<KeyboardServiceImpl, Keyboard>
      keyboard_service_factory_;

  scoped_ptr<ViewsInit> views_init_;

  ViewManager* view_manager_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  KeyboardServiceImpl* keyboard_service_;

  Id target_;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

KeyboardServiceImpl::KeyboardServiceImpl(Keyboard* keyboard)
    : keyboard_(keyboard) {
  keyboard_->set_keyboard_service(this);
}

void KeyboardServiceImpl::SetTarget(uint32_t view_id) {
  keyboard_->set_target(view_id);
}

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::Keyboard);
  return runner.Run(shell_handle);
}
