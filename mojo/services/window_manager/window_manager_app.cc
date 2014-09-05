// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_app.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/aura/aura_init.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/focus_rules.h"
#include "ui/wm/public/activation_client.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::View*);

namespace mojo {

// The aura::Windows we use to track Views don't render, so we don't actually
// need to supply a fully functional WindowDelegate. We do need to provide _a_
// delegate however, otherwise the event dispatcher won't dispatch events to
// these windows. (The aura WindowTargeter won't allow a delegate-less window
// to be the target of an event, since the window delegate is considered the
// "target handler").
class DummyDelegate : public aura::WindowDelegate {
 public:
  DummyDelegate() {}
  virtual ~DummyDelegate() {}

 private:
  // WindowDelegate overrides:
  virtual gfx::Size GetMinimumSize() const OVERRIDE { return gfx::Size(); }
  virtual gfx::Size GetMaximumSize() const OVERRIDE { return gfx::Size(); }
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE { return true; }
  virtual bool CanFocus() OVERRIDE { return true; }
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {}
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {}
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE { return false; }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(DummyDelegate);
};

namespace {

DEFINE_WINDOW_PROPERTY_KEY(View*, kViewKey, NULL);

Id GetIdForWindow(aura::Window* window) {
  return window ? WindowManagerApp::GetViewForWindow(window)->id() : 0;
}

class WMFocusRules : public wm::FocusRules {
 public:
  WMFocusRules() {}
  virtual ~WMFocusRules() {}

 private:
  // Overridden from wm::FocusRules:
  virtual bool IsToplevelWindow(aura::Window* window) const MOJO_OVERRIDE {
    return true;
  }
  virtual bool CanActivateWindow(aura::Window* window) const MOJO_OVERRIDE {
    return true;
  }
  virtual bool CanFocusWindow(aura::Window* window) const MOJO_OVERRIDE {
    return true;
  }
  virtual aura::Window* GetToplevelWindow(
      aura::Window* window) const MOJO_OVERRIDE {
    return window;
  }
  virtual aura::Window* GetActivatableWindow(
      aura::Window* window) const MOJO_OVERRIDE {
    return window;
  }
  virtual aura::Window* GetFocusableWindow(
      aura::Window* window) const MOJO_OVERRIDE {
    return window;
  }
  virtual aura::Window* GetNextActivatableWindow(
      aura::Window* ignore) const MOJO_OVERRIDE {
    return NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(WMFocusRules);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, public:

WindowManagerApp::WindowManagerApp(
    ViewManagerDelegate* view_manager_delegate,
    WindowManagerDelegate* window_manager_delegate)
    : window_manager_service_factory_(this),
      wrapped_view_manager_delegate_(view_manager_delegate),
      wrapped_window_manager_delegate_(window_manager_delegate),
      view_manager_(NULL),
      root_(NULL),
      dummy_delegate_(new DummyDelegate) {
}

WindowManagerApp::~WindowManagerApp() {}

// static
View* WindowManagerApp::GetViewForWindow(aura::Window* window) {
  return window->GetProperty(kViewKey);
}

void WindowManagerApp::AddConnection(WindowManagerServiceImpl* connection) {
  DCHECK(connections_.find(connection) == connections_.end());
  connections_.insert(connection);
}

void WindowManagerApp::RemoveConnection(WindowManagerServiceImpl* connection) {
  DCHECK(connections_.find(connection) != connections_.end());
  connections_.erase(connection);
}

void WindowManagerApp::SetCapture(Id view) {
  capture_client_->capture_client()->SetCapture(GetWindowForViewId(view));
  // TODO(beng): notify connected clients that capture has changed, probably
  //             by implementing some capture-client observer.
}

void WindowManagerApp::FocusWindow(Id view) {
  aura::Window* window = GetWindowForViewId(view);
  DCHECK(window);
  focus_client_->FocusWindow(window);
}

void WindowManagerApp::ActivateWindow(Id view) {
  aura::Window* window = GetWindowForViewId(view);
  DCHECK(window);
  activation_client_->ActivateWindow(window);
}

bool WindowManagerApp::IsReady() const {
  return view_manager_ && root_;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ApplicationDelegate implementation:

void WindowManagerApp::Initialize(ApplicationImpl* impl) {
  aura_init_.reset(new AuraInit);
  view_manager_client_factory_.reset(
      new ViewManagerClientFactory(impl->shell(), this));
}

bool WindowManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(&window_manager_service_factory_);
  connection->AddService(view_manager_client_factory_.get());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewManagerDelegate implementation:

void WindowManagerApp::OnEmbed(ViewManager* view_manager,
                               View* root,
                               ServiceProviderImpl* exported_services,
                               scoped_ptr<ServiceProvider> imported_services) {
  DCHECK(!view_manager_ && !root_);
  view_manager_ = view_manager;
  view_manager_->SetWindowManagerDelegate(this);
  root_ = root;

  window_tree_host_.reset(new WindowTreeHostMojo(root_, this));
  window_tree_host_->window()->SetBounds(root->bounds());
  window_tree_host_->window()->Show();

  RegisterSubtree(root_, window_tree_host_->window());

  capture_client_.reset(
      new wm::ScopedCaptureClient(window_tree_host_->window()));
  wm::FocusController* focus_controller =
      new wm::FocusController(new WMFocusRules);
  activation_client_ = focus_controller;
  focus_client_.reset(focus_controller);
  aura::client::SetFocusClient(window_tree_host_->window(), focus_controller);

  focus_client_->AddObserver(this);
  activation_client_->AddObserver(this);

  if (wrapped_view_manager_delegate_) {
    wrapped_view_manager_delegate_->OnEmbed(
        view_manager, root, exported_services, imported_services.Pass());
  }

  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyReady();
  }
}

void WindowManagerApp::OnViewManagerDisconnected(
    ViewManager* view_manager) {
  DCHECK_EQ(view_manager_, view_manager);
  if (wrapped_view_manager_delegate_)
    wrapped_view_manager_delegate_->OnViewManagerDisconnected(view_manager);
  view_manager_ = NULL;
  base::MessageLoop::current()->Quit();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, WindowManagerDelegate implementation:

void WindowManagerApp::Embed(
    const String& url,
    InterfaceRequest<ServiceProvider> service_provider) {
  if (wrapped_window_manager_delegate_)
    wrapped_window_manager_delegate_->Embed(url, service_provider.Pass());
}

void WindowManagerApp::DispatchEvent(EventPtr event) {
  scoped_ptr<ui::Event> ui_event = event.To<scoped_ptr<ui::Event> >();
  if (ui_event)
    window_tree_host_->SendEventToProcessor(ui_event.get());
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewObserver implementation:

void WindowManagerApp::OnTreeChanged(
    const ViewObserver::TreeChangeParams& params) {
  if (params.receiver != root_)
    return;
  DCHECK(params.old_parent || params.new_parent);
  if (!params.target)
    return;

  if (params.new_parent) {
    if (view_id_to_window_map_.find(params.target->id()) ==
        view_id_to_window_map_.end()) {
      ViewIdToWindowMap::const_iterator it =
          view_id_to_window_map_.find(params.new_parent->id());
      DCHECK(it != view_id_to_window_map_.end());
      RegisterSubtree(params.target, it->second);
    }
  } else if (params.old_parent) {
    UnregisterSubtree(params.target);
  }
}

void WindowManagerApp::OnViewDestroyed(View* view) {
  if (view != root_)
    return;
  aura::Window* window = GetWindowForViewId(view->id());
  window->RemovePreTargetHandler(this);
  root_ = NULL;
  STLDeleteValues(&view_id_to_window_map_);
  if (focus_client_.get())
    focus_client_->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  window_tree_host_.reset();
}

void WindowManagerApp::OnViewBoundsChanged(View* view,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  aura::Window* window = GetWindowForViewId(view->id());
  window->SetBounds(new_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, WindowTreeHostMojoDelegate implementation:

void WindowManagerApp::CompositorContentsChanged(const SkBitmap& bitmap) {
  // We draw nothing.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ui::EventHandler implementation:

void WindowManagerApp::OnEvent(ui::Event* event) {
  aura::Window* window = static_cast<aura::Window*>(event->target());
  view_manager_->DispatchEvent(GetViewForWindow(window), Event::From(*event));
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, aura::client::FocusChangeObserver implementation:

void WindowManagerApp::OnWindowFocused(aura::Window* gained_focus,
                                       aura::Window* lost_focus) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyViewFocused(GetIdForWindow(gained_focus),
                             GetIdForWindow(lost_focus));
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, aura::client::ActivationChangeObserver implementation:

void WindowManagerApp::OnWindowActivated(aura::Window* gained_active,
                                         aura::Window* lost_active) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyWindowActivated(GetIdForWindow(gained_active),
                                 GetIdForWindow(lost_active));
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, private:

aura::Window* WindowManagerApp::GetWindowForViewId(Id view) const {
  ViewIdToWindowMap::const_iterator it = view_id_to_window_map_.find(view);
  return it != view_id_to_window_map_.end() ? it->second : NULL;
}

void WindowManagerApp::RegisterSubtree(View* view, aura::Window* parent) {
  view->AddObserver(this);
  DCHECK(view_id_to_window_map_.find(view->id()) ==
         view_id_to_window_map_.end());
  aura::Window* window = new aura::Window(dummy_delegate_.get());
  window->set_id(view->id());
  window->SetProperty(kViewKey, view);
  // All events pass through the root during dispatch, so we only need a handler
  // installed there.
  if (view == root_)
    window->AddPreTargetHandler(this);
  parent->AddChild(window);
  window->SetBounds(view->bounds());
  window->Show();
  view_id_to_window_map_[view->id()] = window;
  View::Children::const_iterator it = view->children().begin();
  for (; it != view->children().end(); ++it)
    RegisterSubtree(*it, window);
}

void WindowManagerApp::UnregisterSubtree(View* view) {
  view->RemoveObserver(this);
  ViewIdToWindowMap::iterator it = view_id_to_window_map_.find(view->id());
  DCHECK(it != view_id_to_window_map_.end());
  scoped_ptr<aura::Window> window(it->second);
  view_id_to_window_map_.erase(it);
  View::Children::const_iterator child = view->children().begin();
  for (; child != view->children().end(); ++child)
    UnregisterSubtree(*child);
}

}  // namespace mojo
