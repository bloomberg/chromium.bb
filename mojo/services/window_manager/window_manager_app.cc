// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_app.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/aura/aura_init.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/focus_rules.h"
#include "ui/wm/public/activation_client.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::View*);

namespace mojo {
namespace {

DEFINE_WINDOW_PROPERTY_KEY(View*, kViewKey, NULL);

Id GetIdForWindow(aura::Window* window) {
  return window ? window->GetProperty(kViewKey)->id() : 0;
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

WindowManagerApp::WindowManagerApp(ViewManagerDelegate* delegate)
    : window_manager_service_factory_(this),
      wrapped_delegate_(delegate),
      view_manager_(NULL),
      view_manager_client_factory_(this),
      root_(NULL) {
}

WindowManagerApp::~WindowManagerApp() {}

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
}

bool WindowManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(&window_manager_service_factory_);
  connection->AddService(&view_manager_client_factory_);
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
  root_ = root;
  root_->AddObserver(this);

  window_tree_host_.reset(new WindowTreeHostMojo(root_, this));

  RegisterSubtree(root_->id(), window_tree_host_->window());

  capture_client_.reset(
      new wm::ScopedCaptureClient(window_tree_host_->window()));
  wm::FocusController* focus_controller =
      new wm::FocusController(new WMFocusRules);
  activation_client_ = focus_controller;
  focus_client_.reset(focus_controller);

  focus_client_->AddObserver(this);
  activation_client_->AddObserver(this);

  if (wrapped_delegate_) {
    wrapped_delegate_->OnEmbed(view_manager, root, exported_services,
                               imported_services.Pass());
  }

  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyReady();
  }
}

void WindowManagerApp::OnViewManagerDisconnected(
    ViewManager* view_manager) {
  DCHECK_EQ(view_manager_, view_manager);
  if (wrapped_delegate_)
    wrapped_delegate_->OnViewManagerDisconnected(view_manager);
  view_manager_ = NULL;
  base::MessageLoop::current()->Quit();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewObserver implementation:

void WindowManagerApp::OnTreeChanged(
    const ViewObserver::TreeChangeParams& params) {
  DCHECK_EQ(params.receiver, root_);
  DCHECK(params.old_parent || params.new_parent);
  if (!params.target)
    return;

  if (params.new_parent) {
    if (view_id_to_window_map_.find(params.target->id()) ==
        view_id_to_window_map_.end()) {
      ViewIdToWindowMap::const_iterator it =
          view_id_to_window_map_.find(params.new_parent->id());
      DCHECK(it != view_id_to_window_map_.end());
      RegisterSubtree(params.target->id(), it->second);
    }
  } else if (params.old_parent) {
    UnregisterSubtree(params.target->id());
  }
}

void WindowManagerApp::OnViewDestroyed(View* view) {
  root_ = NULL;
  STLDeleteValues(&view_id_to_window_map_);
  if (focus_client_.get())
    focus_client_->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  window_tree_host_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, WindowTreeHostMojoDelegate implementation:

void WindowManagerApp::CompositorContentsChanged(const SkBitmap& bitmap) {
  // We draw nothing.
  NOTREACHED();
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

void WindowManagerApp::RegisterSubtree(Id id,
                                       aura::Window* parent) {
  View* view = view_manager_->GetViewById(id);
  DCHECK(view_id_to_window_map_.find(id) == view_id_to_window_map_.end());
  aura::Window* window = new aura::Window(NULL);
  window->SetProperty(kViewKey, view);
  parent->AddChild(window);
  view_id_to_window_map_[id] = window;
  View::Children::const_iterator it = view->children().begin();
  for (; it != view->children().end(); ++it)
    RegisterSubtree((*it)->id(), window);
}

void WindowManagerApp::UnregisterSubtree(Id id) {
  View* view = view_manager_->GetViewById(id);
  ViewIdToWindowMap::iterator it = view_id_to_window_map_.find(id);
  DCHECK(it != view_id_to_window_map_.end());
  scoped_ptr<aura::Window> window(it->second);
  view_id_to_window_map_.erase(it);
  View::Children::const_iterator child = view->children().begin();
  for (; child != view->children().end(); ++child)
    UnregisterSubtree((*child)->id());
}

}  // namespace mojo
