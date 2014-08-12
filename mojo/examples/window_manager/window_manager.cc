// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/examples/keyboard/keyboard.mojom.h"
#include "mojo/examples/window_manager/debug_panel.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/window_manager_delegate.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"
#include "mojo/services/public/interfaces/launcher/launcher.mojom.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/views/views_init.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined CreateWindow
#undef CreateWindow
#endif

namespace mojo {
namespace examples {

class WindowManager;

namespace {

const int kBorderInset = 25;
const int kControlPanelWidth = 200;
const int kTextfieldHeight = 25;

}  // namespace

class WindowManagerConnection : public InterfaceImpl<IWindowManager> {
 public:
  explicit WindowManagerConnection(WindowManager* window_manager)
      : window_manager_(window_manager) {}
  virtual ~WindowManagerConnection() {}

 private:
  // Overridden from IWindowManager:
  virtual void CloseWindow(Id view_id) OVERRIDE;
  virtual void ShowKeyboard(Id view_id, RectPtr bounds) OVERRIDE;
  virtual void HideKeyboard(Id view_id) OVERRIDE;

  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

class NavigatorHostImpl : public InterfaceImpl<NavigatorHost> {
 public:
  explicit NavigatorHostImpl(WindowManager* window_manager)
      : window_manager_(window_manager) {}
  virtual ~NavigatorHostImpl() {
  }

 private:
  virtual void DidNavigateLocally(uint32 source_view_id,
                                  const mojo::String& url) OVERRIDE;
  virtual void RequestNavigate(
      uint32 source_view_id,
      Target target,
      NavigationDetailsPtr nav_details) OVERRIDE;
  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorHostImpl);
};

class KeyboardManager : public KeyboardClient,
                        public ViewObserver {
 public:
  KeyboardManager() : view_manager_(NULL), view_(NULL) {
  }
  virtual ~KeyboardManager() {
    if (view_)
      view_->parent()->RemoveObserver(this);
  }

  View* view() { return view_; }

  void Init(ApplicationImpl* application,
            ViewManager* view_manager,
            View* parent,
            const gfx::Rect& bounds) {
    view_manager_ = view_manager;
    view_ = View::Create(view_manager);
    view_->SetBounds(bounds);
    parent->AddChild(view_);
    view_->Embed("mojo:mojo_keyboard");
    application->ConnectToService("mojo:mojo_keyboard", &keyboard_service_);
    keyboard_service_.set_client(this);
    parent->AddObserver(this);
  }

  void Show(Id view_id, const gfx::Rect& bounds) {
    keyboard_service_->SetTarget(view_id);
    view_->SetVisible(true);
  }

  void Hide(Id view_id) {
    keyboard_service_->SetTarget(0);
    view_->SetVisible(false);
  }

 private:
  // KeyboardClient:
  virtual void OnKeyboardEvent(Id view_id,
                               int32_t code,
                               int32_t flags) OVERRIDE {
    View* view = view_manager_->GetViewById(view_id);
    if (!view)
      return;
#if defined(OS_WIN)
    const bool is_char = code != ui::VKEY_BACK && code != ui::VKEY_RETURN;
#else
    const bool is_char = false;
#endif
    if (is_char) {
      view_manager_->DispatchEvent(
          view,
          Event::From(ui::KeyEvent(ui::ET_KEY_PRESSED,
                                   static_cast<ui::KeyboardCode>(code),
                                   flags)));
    } else {
      view_manager_->DispatchEvent(
          view,
          Event::From(ui::KeyEvent(static_cast<base::char16>(code),
                                   static_cast<ui::KeyboardCode>(code),
                                   flags)));
    }
    view_manager_->DispatchEvent(
        view,
        Event::From(ui::KeyEvent(ui::ET_KEY_RELEASED,
                                 static_cast<ui::KeyboardCode>(code),
                                 flags)));
  }

  // Overridden from ViewObserver:
  virtual void OnViewBoundsChanged(View* parent,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    gfx::Rect keyboard_bounds(view_->bounds());
    keyboard_bounds.set_y(new_bounds.bottom() - keyboard_bounds.height());
    keyboard_bounds.set_width(keyboard_bounds.width() +
                              new_bounds.width() - old_bounds.width());
    view_->SetBounds(keyboard_bounds);
  }
  virtual void OnViewDestroyed(View* parent) OVERRIDE {
    DCHECK_EQ(parent, view_->parent());
    parent->RemoveObserver(this);
    view_ = NULL;
  }

  KeyboardServicePtr keyboard_service_;
  ViewManager* view_manager_;

  // View the keyboard is attached to.
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardManager);
};

class RootLayoutManager : public ViewObserver {
 public:
  RootLayoutManager(ViewManager* view_manager,
                    View* root,
                    Id content_view_id,
                    Id launcher_ui_view_id,
                    Id control_panel_view_id)
      : root_(root),
        view_manager_(view_manager),
        content_view_id_(content_view_id),
        launcher_ui_view_id_(launcher_ui_view_id),
        control_panel_view_id_(control_panel_view_id) {}
  virtual ~RootLayoutManager() {
    if (root_)
      root_->RemoveObserver(this);
  }

 private:
  // Overridden from ViewObserver:
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    DCHECK_EQ(view, root_);

    View* content_view = view_manager_->GetViewById(content_view_id_);
    content_view->SetBounds(new_bounds);
    // Force the view's bitmap to be recreated
    content_view->SetColor(SK_ColorBLUE);

    int delta_width = new_bounds.width() - old_bounds.width();
    int delta_height = new_bounds.height() - old_bounds.height();

    View* launcher_ui_view =
        view_manager_->GetViewById(launcher_ui_view_id_);
    gfx::Rect launcher_ui_bounds(launcher_ui_view->bounds());
    launcher_ui_bounds.set_width(launcher_ui_bounds.width() + delta_width);
    launcher_ui_view->SetBounds(launcher_ui_bounds);

    View* control_panel_view =
        view_manager_->GetViewById(control_panel_view_id_);
    gfx::Rect control_panel_bounds(control_panel_view->bounds());
    control_panel_bounds.set_x(control_panel_bounds.x() + delta_width);
    control_panel_view->SetBounds(control_panel_bounds);

    const View::Children& content_views = content_view->children();
    View::Children::const_iterator iter = content_views.begin();
    for(; iter != content_views.end(); ++iter) {
      View* view = *iter;
      if (view->id() == control_panel_view->id() ||
          view->id() == launcher_ui_view->id())
        continue;
      gfx::Rect view_bounds(view->bounds());
      view_bounds.set_width(view_bounds.width() + delta_width);
      view_bounds.set_height(view_bounds.height() + delta_height);
      view->SetBounds(view_bounds);
    }
  }
  virtual void OnViewDestroyed(View* view) OVERRIDE {
    DCHECK_EQ(view, root_);
    root_->RemoveObserver(this);
    root_ = NULL;
  }

  View* root_;
  ViewManager* view_manager_;
  const Id content_view_id_;
  const Id launcher_ui_view_id_;
  const Id control_panel_view_id_;

  DISALLOW_COPY_AND_ASSIGN(RootLayoutManager);
};

class WindowManager
    : public ApplicationDelegate,
      public DebugPanel::Delegate,
      public ViewManagerDelegate,
      public WindowManagerDelegate {
 public:
  WindowManager()
      : window_manager_factory_(this),
        navigator_host_factory_(this),
        launcher_ui_(NULL),
        view_manager_(NULL),
        view_manager_client_factory_(this),
        app_(NULL) {}

  virtual ~WindowManager() {}

  void CloseWindow(Id view_id) {
    View* view = view_manager_->GetViewById(view_id);
    DCHECK(view);
    std::vector<View*>::iterator iter =
        std::find(windows_.begin(), windows_.end(), view);
    DCHECK(iter != windows_.end());
    windows_.erase(iter);
    view->Destroy();
  }

  void ShowKeyboard(Id view_id, const gfx::Rect& bounds) {
    // TODO: this needs to validate |view_id|. That is, it shouldn't assume
    // |view_id| is valid and it also needs to make sure the client that sent
    // this really owns |view_id|.
    // TODO: honor |bounds|.
    if (!keyboard_manager_) {
      keyboard_manager_.reset(new KeyboardManager);
      View* parent = view_manager_->GetRoots().back();
      int ideal_height = 200;
      // TODO(sky): 10 is a bit of a hack here. There is a bug that causes
      // white strips to appear when 0 is used. Figure this out!
      const gfx::Rect keyboard_bounds(
          10, parent->bounds().height() - ideal_height,
          parent->bounds().width() - 20, ideal_height);
      keyboard_manager_->Init(app_, view_manager_, parent, keyboard_bounds);
    }
    keyboard_manager_->Show(view_id, bounds);
  }

  void HideKeyboard(Id view_id) {
    // See comment in ShowKeyboard() about validating args.
    if (keyboard_manager_)
      keyboard_manager_->Hide(view_id);
  }

  void DidNavigateLocally(uint32 source_view_id, const mojo::String& url) {
    LOG(ERROR) << "DidNavigateLocally: source_view_id: " << source_view_id
               << " url: " << url.To<std::string>();
  }

  // Overridden from DebugPanel::Delegate:
  virtual void CloseTopWindow() OVERRIDE {
    if (!windows_.empty())
      CloseWindow(windows_.back()->id());
  }

  virtual void RequestNavigate(
    uint32 source_view_id,
    Target target,
    NavigationDetailsPtr nav_details) OVERRIDE {
    launcher_->Launch(nav_details.Pass(),
                      base::Bind(&WindowManager::OnLaunch,
                                 base::Unretained(this),
                                 source_view_id,
                                 target));
  }

 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    app_ = app;
    app->ConnectToService("mojo:mojo_launcher", &launcher_);
    views_init_.reset(new ViewsInit);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&window_manager_factory_);
    connection->AddService(&navigator_host_factory_);
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    DCHECK(!view_manager_);
    view_manager_ = view_manager;
    view_manager_->SetWindowManagerDelegate(this);

    View* view = View::Create(view_manager_);
    root->AddChild(view);
    view->SetBounds(gfx::Rect(root->bounds().size()));
    content_view_id_ = view->id();

    root->SetColor(SK_ColorBLUE);

    Id launcher_ui_id = CreateLauncherUI();
    Id control_panel_id = CreateControlPanel(view);

    root_layout_manager_.reset(
        new RootLayoutManager(view_manager, root,
                              content_view_id_,
                              launcher_ui_id,
                              control_panel_id));
    root->AddObserver(root_layout_manager_.get());
  }
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    DCHECK_EQ(view_manager_, view_manager);
    view_manager_ = NULL;
    base::MessageLoop::current()->Quit();
  }

  // Overridden from WindowManagerDelegate:
  virtual void Embed(
      const String& url,
      InterfaceRequest<ServiceProvider> service_provider) OVERRIDE {
    CreateWindow(url,
                 NavigationDetailsPtr().Pass(),
                 ResponseDetailsPtr().Pass());
  }
  virtual void DispatchEvent(View* target, EventPtr event) OVERRIDE {
    // TODO(beng): More sophisticated focus handling than this is required!
    if (event->action == EVENT_TYPE_MOUSE_PRESSED &&
        !IsDescendantOfKeyboard(target)) {
      target->SetFocus();
    }
    view_manager_->DispatchEvent(target, event.Pass());
  }

  void OnLaunch(
      uint32 source_view_id,
      Target requested_target,
      const mojo::String& handler_url,
      const mojo::String& view_url,
      ResponseDetailsPtr response) {
    // TODO(mpcomplete): This seems to be unused in favor of |response|. We
    // might need to use it (and fill it in properly, with method, etc) if we
    // need to preserve that information.
    NavigationDetailsPtr nav_details(NavigationDetails::New());
    nav_details->request->url = view_url;

    Target target = debug_panel_->navigation_target();
    if (target == TARGET_DEFAULT) {
      if (requested_target != TARGET_DEFAULT) {
        target = requested_target;
      } else {
        // TODO(aa): Should be TARGET_NEW_NODE if source origin and dest origin
        // are different?
        target = TARGET_SOURCE_NODE;
      }
    }

    View* dest_view = NULL;
    if (target == TARGET_SOURCE_NODE) {
      View* source_view = view_manager_->GetViewById(source_view_id);
      bool app_initiated = std::find(windows_.begin(), windows_.end(),
                                     source_view) != windows_.end();
      if (app_initiated)
        dest_view = source_view;
      else if (!windows_.empty())
        dest_view = windows_.back();
    }

    if (dest_view)
      Embed(dest_view, handler_url, nav_details.Pass(), response.Pass());
    else
      CreateWindow(handler_url, nav_details.Pass(), response.Pass());
  }

  // TODO(beng): proper layout manager!!
  Id CreateLauncherUI() {
    NavigationDetailsPtr nav_details;
    ResponseDetailsPtr response;
    View* view = view_manager_->GetViewById(content_view_id_);
    gfx::Rect bounds = view->bounds();
    bounds.Inset(kBorderInset, kBorderInset);
    bounds.set_height(kTextfieldHeight);
    launcher_ui_ = CreateChild(content_view_id_, "mojo:mojo_browser", bounds,
                               nav_details.Pass(), response.Pass());
    return launcher_ui_->id();
  }

  void CreateWindow(const std::string& handler_url,
                    NavigationDetailsPtr nav_details,
                    ResponseDetailsPtr response) {
    View* view = view_manager_->GetViewById(content_view_id_);
    gfx::Rect bounds(kBorderInset,
                     2 * kBorderInset + kTextfieldHeight,
                     view->bounds().width() - 3 * kBorderInset -
                         kControlPanelWidth,
                     view->bounds().height() -
                         (3 * kBorderInset + kTextfieldHeight));
    if (!windows_.empty()) {
      gfx::Point position = windows_.back()->bounds().origin();
      position.Offset(35, 35);
      bounds.set_origin(position);
    }
    windows_.push_back(CreateChild(content_view_id_, handler_url, bounds,
                                   nav_details.Pass(), response.Pass()));
  }

  View* CreateChild(Id parent_id,
                    const std::string& url,
                    const gfx::Rect& bounds,
                    NavigationDetailsPtr nav_details,
                    ResponseDetailsPtr response) {
    View* view = view_manager_->GetViewById(parent_id);
    View* embedded = View::Create(view_manager_);
    view->AddChild(embedded);
    embedded->SetBounds(bounds);
    Embed(embedded, url, nav_details.Pass(), response.Pass());
    embedded->SetFocus();
    return embedded;
  }

  void Embed(View* view, const std::string& app_url,
             NavigationDetailsPtr nav_details,
             ResponseDetailsPtr response) {
    view->Embed(app_url);
    if (nav_details) {
      NavigatorPtr navigator;
      app_->ConnectToService(app_url, &navigator);
      navigator->Navigate(view->id(), nav_details.Pass(), response.Pass());
    }
  }

  bool IsDescendantOfKeyboard(View* target) {
    return keyboard_manager_.get() &&
        keyboard_manager_->view()->Contains(target);
  }

  Id CreateControlPanel(View* root) {
    View* view = View::Create(view_manager_);
    root->AddChild(view);

    gfx::Rect bounds(root->bounds().width() - kControlPanelWidth -
                         kBorderInset,
                     kBorderInset * 2 + kTextfieldHeight,
                     kControlPanelWidth,
                     root->bounds().height() - kBorderInset * 3 -
                         kTextfieldHeight);
    view->SetBounds(bounds);

    debug_panel_ = new DebugPanel(this, view);
    return view->id();
  }

  InterfaceFactoryImplWithContext<WindowManagerConnection, WindowManager>
      window_manager_factory_;
  InterfaceFactoryImplWithContext<NavigatorHostImpl, WindowManager>
      navigator_host_factory_;

  scoped_ptr<ViewsInit> views_init_;
  DebugPanel* debug_panel_;
  LauncherPtr launcher_;
  View* launcher_ui_;
  std::vector<View*> windows_;
  ViewManager* view_manager_;
  ViewManagerClientFactory view_manager_client_factory_;
  scoped_ptr<RootLayoutManager> root_layout_manager_;

  // Id of the view most content is added to. The keyboard is NOT added here.
  Id content_view_id_;

  scoped_ptr<KeyboardManager> keyboard_manager_;
  ApplicationImpl* app_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

void WindowManagerConnection::CloseWindow(Id view_id) {
  window_manager_->CloseWindow(view_id);
}

void WindowManagerConnection::ShowKeyboard(Id view_id, RectPtr bounds) {
  window_manager_->ShowKeyboard(view_id, bounds.To<gfx::Rect>());
}

void WindowManagerConnection::HideKeyboard(Id view_id) {
  window_manager_->HideKeyboard(view_id);
}

void NavigatorHostImpl::DidNavigateLocally(uint32 source_view_id,
                                           const mojo::String& url) {
  window_manager_->DidNavigateLocally(source_view_id, url);
}

void NavigatorHostImpl::RequestNavigate(
    uint32 source_view_id,
    Target target,
    NavigationDetailsPtr nav_details) {
  window_manager_->RequestNavigate(source_view_id, target, nav_details.Pass());
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::WindowManager;
}

}  // namespace mojo
