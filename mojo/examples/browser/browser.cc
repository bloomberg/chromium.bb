// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace mojo {
namespace examples {

class BrowserLayoutManager : public views::LayoutManager {
 public:
  BrowserLayoutManager() {}
  virtual ~BrowserLayoutManager() {}

 private:
  // Overridden from views::LayoutManager:
  virtual void Layout(views::View* host) OVERRIDE {
    // Browser view has one child, a text input field.
    DCHECK_EQ(1, host->child_count());
    views::View* text_field = host->child_at(0);
    gfx::Size ps = text_field->GetPreferredSize();
    text_field->SetBoundsRect(gfx::Rect(host->width(), ps.height()));
  }
  virtual gfx::Size GetPreferredSize(const views::View* host) const OVERRIDE {
    return gfx::Size();
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserLayoutManager);
};

// KeyboardManager handles notifying the windowmanager when views are focused.
// To use create one and KeyboardManager will take care of all other details.
//
// TODO(sky): it would be nice if this were put in NativeWidgetViewManager, but
// that requires NativeWidgetViewManager to take an IWindowManager. That may be
// desirable anyway...
class KeyboardManager
    : public views::FocusChangeListener,
      public ui::EventHandler,
      public views::WidgetObserver {
 public:
  KeyboardManager(views::Widget* widget,
                  IWindowManager* window_manager,
                  Node* node)
      : widget_(widget),
        window_manager_(window_manager),
        node_(node),
        last_view_id_(0),
        focused_view_(NULL) {
    widget_->GetFocusManager()->AddFocusChangeListener(this);
    widget_->AddObserver(this);
    widget_->GetNativeView()->AddPostTargetHandler(this);
  }

 private:
  virtual ~KeyboardManager() {
    widget_->GetFocusManager()->RemoveFocusChangeListener(this);
    widget_->GetNativeView()->RemovePostTargetHandler(this);
    widget_->RemoveObserver(this);

    HideKeyboard();
  }

  void ShowKeyboard(views::View* view) {
    if (focused_view_ == view)
      return;

    const gfx::Rect bounds_in_widget =
        view->ConvertRectToWidget(gfx::Rect(view->bounds().size()));
    last_view_id_ = node_->active_view()->id();
    window_manager_->ShowKeyboard(last_view_id_,
                                  Rect::From(bounds_in_widget));
    // TODO(sky): listen for view to be removed.
    focused_view_ = view;
  }

  void HideKeyboard() {
    if (!focused_view_)
      return;

    window_manager_->HideKeyboard(last_view_id_);
    last_view_id_ = 0;
    focused_view_ = NULL;
  }

  // views::FocusChangeListener:
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) OVERRIDE {
  }
  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) OVERRIDE {
    if (focused_view_ && focused_now != focused_view_)
      HideKeyboard();
  }

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    views::View* focused_now = widget_->GetFocusManager()->GetFocusedView();
    if (focused_now &&
        focused_now->GetClassName() == views::Textfield::kViewClassName &&
        (event->flags() & ui::EF_FROM_TOUCH) != 0) {
      ShowKeyboard(focused_now);
    }
  }

  // views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    delete this;
  }

  views::Widget* widget_;
  IWindowManager* window_manager_;
  Node* node_;
  Id last_view_id_;
  views::View* focused_view_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardManager);
};

// This is the basics of creating a views widget with a textfield.
// TODO: cleanup!
class Browser : public ApplicationDelegate,
                public ViewManagerDelegate,
                public views::TextfieldController,
                public NodeObserver {
 public:
  Browser()
      : view_manager_(NULL),
        view_manager_client_factory_(this),
        root_(NULL),
        widget_(NULL) {}

  virtual ~Browser() {
    if (root_)
      root_->RemoveObserver(this);
  }

 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    views_init_.reset(new ViewsInit);
    app->ConnectToService("mojo:mojo_window_manager", &navigator_host_);
    app->ConnectToService("mojo:mojo_window_manager", &window_manager_);
 }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  void CreateWidget(Node* node) {
    views::Textfield* textfield = new views::Textfield;
    textfield->set_controller(this);

    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->set_background(
        views::Background::CreateSolidBackground(SK_ColorBLUE));
    widget_delegate->GetContentsView()->AddChildView(textfield);
    widget_delegate->GetContentsView()->SetLayoutManager(
        new BrowserLayoutManager);

    widget_ = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.native_widget = new NativeWidgetViewManager(widget_, node);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(node->bounds().width(), node->bounds().height());
    widget_->Init(params);
    // KeyboardManager handles deleting itself when the widget is destroyed.
    new KeyboardManager(widget_, window_manager_.get(), node);
    widget_->Show();
    textfield->RequestFocus();
  }

  // ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       Node* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    // TODO: deal with OnEmbed() being invoked multiple times.
    view_manager_ = view_manager;
    root_ = root;
    root_->AddObserver(this);
    root_->SetActiveView(View::Create(view_manager));
    root_->SetFocus();
    CreateWidget(root_);
  }
  virtual void OnViewManagerDisconnected(
      ViewManager* view_manager) OVERRIDE {
    DCHECK_EQ(view_manager_, view_manager);
    view_manager_ = NULL;
    base::MessageLoop::current()->Quit();
  }

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      GURL url(sender->text());
      printf("User entered this URL: %s\n", url.spec().c_str());
      NavigationDetailsPtr nav_details(NavigationDetails::New());
      nav_details->request->url = String::From(url);
      navigator_host_->RequestNavigate(view_manager_->GetRoots().front()->id(),
                                       TARGET_NEW_NODE,
                                       nav_details.Pass());
    }
    return false;
  }

  // NodeObserver:
  virtual void OnNodeFocusChanged(Node* gained_focus,
                                  Node* lost_focus) OVERRIDE {
    aura::client::FocusClient* focus_client =
        aura::client::GetFocusClient(widget_->GetNativeView());
    if (lost_focus == root_)
      focus_client->FocusWindow(NULL);
    else if (gained_focus == root_)
      focus_client->FocusWindow(widget_->GetNativeView());
  }
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    DCHECK_EQ(root_, node);
    node->RemoveObserver(this);
    root_ = NULL;
  }

  scoped_ptr<ViewsInit> views_init_;

  ViewManager* view_manager_;
  ViewManagerClientFactory view_manager_client_factory_;
  Node* root_;
  views::Widget* widget_;
  NavigatorHostPtr navigator_host_;
  IWindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::Browser;
}

}  // namespace mojo
