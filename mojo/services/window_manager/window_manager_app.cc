// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_app.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/window_manager/capture_controller.h"
#include "mojo/services/window_manager/focus_controller.h"
#include "mojo/services/window_manager/focus_rules.h"
#include "mojo/services/window_manager/hit_test.h"
#include "mojo/services/window_manager/view_event_dispatcher.h"
#include "mojo/services/window_manager/view_target.h"
#include "mojo/services/window_manager/view_targeter.h"
#include "mojo/services/window_manager/window_manager_delegate.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager.h"

using mojo::ApplicationConnection;
using mojo::Id;
using mojo::ServiceProvider;
using mojo::View;
using mojo::WindowManager;

namespace window_manager {

namespace {

Id GetIdForView(View* view) {
  return view ? view->id() : 0;
}

}  // namespace

// Used for calls to Embed() that occur before we've connected to the
// ViewManager.
struct WindowManagerApp::PendingEmbed {
  mojo::String url;
  mojo::InterfaceRequest<ServiceProvider> services;
  mojo::ServiceProviderPtr exposed_services;
};

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, public:

WindowManagerApp::WindowManagerApp(
    ViewManagerDelegate* view_manager_delegate,
    WindowManagerDelegate* window_manager_delegate)
    : shell_(nullptr),
      wrapped_view_manager_delegate_(view_manager_delegate),
      window_manager_delegate_(window_manager_delegate),
      root_(nullptr) {
}

WindowManagerApp::~WindowManagerApp() {
  // TODO(msw|sky): Should this destructor explicitly delete the ViewManager?
  mojo::ViewManager* cached_view_manager = view_manager();
  for (RegisteredViewIdSet::const_iterator it = registered_view_id_set_.begin();
       cached_view_manager && it != registered_view_id_set_.end(); ++it) {
    View* view = cached_view_manager->GetViewById(*it);
    if (view && view == root_)
      root_ = nullptr;
    if (view)
      view->RemoveObserver(this);
  }
  registered_view_id_set_.clear();
  DCHECK(!root_);

  STLDeleteElements(&connections_);
}

void WindowManagerApp::AddConnection(WindowManagerImpl* connection) {
  DCHECK(connections_.find(connection) == connections_.end());
  connections_.insert(connection);
}

void WindowManagerApp::RemoveConnection(WindowManagerImpl* connection) {
  DCHECK(connections_.find(connection) != connections_.end());
  connections_.erase(connection);
}

bool WindowManagerApp::SetCapture(Id view_id) {
  View* view = view_manager()->GetViewById(view_id);
  return view && SetCaptureImpl(view);
}

bool WindowManagerApp::FocusWindow(Id view_id) {
  View* view = view_manager()->GetViewById(view_id);
  return view && FocusWindowImpl(view);
}

bool WindowManagerApp::ActivateWindow(Id view_id) {
  View* view = view_manager()->GetViewById(view_id);
  return view && ActivateWindowImpl(view);
}

bool WindowManagerApp::IsReady() const {
  return !!root_;
}

void WindowManagerApp::InitFocus(scoped_ptr<FocusRules> rules) {
  DCHECK(root_);

  focus_controller_.reset(new FocusController(rules.Pass()));
  focus_controller_->AddObserver(this);
  SetFocusController(root_, focus_controller_.get());

  capture_controller_.reset(new CaptureController);
  capture_controller_->AddObserver(this);
  SetCaptureController(root_, capture_controller_.get());
}

void WindowManagerApp::Embed(
    const mojo::String& url,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  if (view_manager()) {
    window_manager_delegate_->Embed(url, services.Pass(),
                                    exposed_services.Pass());
    return;
  }
  scoped_ptr<PendingEmbed> pending_embed(new PendingEmbed);
  pending_embed->url = url;
  pending_embed->services = services.Pass();
  pending_embed->exposed_services = exposed_services.Pass();
  pending_embeds_.push_back(pending_embed.release());
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ApplicationDelegate implementation:

void WindowManagerApp::Initialize(mojo::ApplicationImpl* impl) {
  shell_ = impl->shell();
  LaunchViewManager(impl);
}

bool WindowManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<WindowManager>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewManagerDelegate implementation:

void WindowManagerApp::OnEmbed(
    View* root,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  DCHECK(!root_);
  root_ = root;

  view_event_dispatcher_.reset(new ViewEventDispatcher);

  RegisterSubtree(root_);

  if (wrapped_view_manager_delegate_) {
    wrapped_view_manager_delegate_->OnEmbed(root, services.Pass(),
                                            exposed_services.Pass());
  }

  for (PendingEmbed* pending_embed : pending_embeds_) {
    Embed(pending_embed->url, pending_embed->services.Pass(),
          pending_embed->exposed_services.Pass());
  }
  pending_embeds_.clear();
}

void WindowManagerApp::OnViewManagerDisconnected(
    mojo::ViewManager* view_manager) {
  if (wrapped_view_manager_delegate_)
    wrapped_view_manager_delegate_->OnViewManagerDisconnected(view_manager);

  base::MessageLoop* message_loop = base::MessageLoop::current();
  if (message_loop && message_loop->is_running())
    message_loop->Quit();
}

bool WindowManagerApp::OnPerformAction(mojo::View* view,
                                       const std::string& action) {
  if (!view)
    return false;
  if (action == "capture")
    return SetCaptureImpl(view);
  if (action == "focus")
    return FocusWindowImpl(view);
  else if (action == "activate")
    return ActivateWindowImpl(view);
  return false;
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
    if (registered_view_id_set_.find(params.target->id()) ==
        registered_view_id_set_.end()) {
      RegisteredViewIdSet::const_iterator it =
          registered_view_id_set_.find(params.new_parent->id());
      DCHECK(it != registered_view_id_set_.end());
      RegisterSubtree(params.target);
    }
  } else if (params.old_parent) {
    UnregisterSubtree(params.target);
  }
}

void WindowManagerApp::OnViewDestroying(View* view) {
  Unregister(view);
  if (view == root_) {
    root_ = nullptr;
    if (focus_controller_)
      focus_controller_->RemoveObserver(this);
    if (capture_controller_)
      capture_controller_->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ui::EventHandler implementation:

void WindowManagerApp::OnEvent(ui::Event* event) {
  if (!window_manager_client_)
    return;

  View* view = static_cast<ViewTarget*>(event->target())->view();
  if (!view)
    return;

  if (event->IsKeyEvent()) {
    const ui::KeyEvent* key_event = static_cast<const ui::KeyEvent*>(event);
    if (key_event->type() == ui::ET_KEY_PRESSED) {
      ui::Accelerator accelerator = ConvertEventToAccelerator(key_event);
      if (accelerator_manager_.Process(accelerator))
        return;
    }
  }

  if (focus_controller_)
    focus_controller_->OnEvent(event);

  window_manager_client_->DispatchInputEventToView(view->id(),
                                                   mojo::Event::From(*event));
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, mojo::FocusControllerObserver implementation:

void WindowManagerApp::OnFocused(View* gained_focus) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyViewFocused(GetIdForView(gained_focus));
  }
}

void WindowManagerApp::OnActivated(View* gained_active) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyWindowActivated(GetIdForView(gained_active));
  }
  if (gained_active)
    gained_active->MoveToFront();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, mojo::CaptureControllerObserver implementation:

void WindowManagerApp::OnCaptureChanged(View* gained_capture) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyCaptureChanged(GetIdForView(gained_capture));
  }
  if (gained_capture)
    gained_capture->MoveToFront();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, private:

bool WindowManagerApp::SetCaptureImpl(View* view) {
  CHECK(view);
  capture_controller_->SetCapture(view);
  return capture_controller_->GetCapture() == view;
}

bool WindowManagerApp::FocusWindowImpl(View* view) {
  CHECK(view);
  focus_controller_->FocusView(view);
  return focus_controller_->GetFocusedView() == view;
}

bool WindowManagerApp::ActivateWindowImpl(View* view) {
  CHECK(view);
  focus_controller_->ActivateView(view);
  return focus_controller_->GetActiveView() == view;
}

void WindowManagerApp::RegisterSubtree(View* view) {
  view->AddObserver(this);
  DCHECK(registered_view_id_set_.find(view->id()) ==
         registered_view_id_set_.end());
  // All events pass through the root during dispatch, so we only need a handler
  // installed there.
  if (view == root_) {
    ViewTarget* target = ViewTarget::TargetFromView(view);
    target->SetEventTargeter(scoped_ptr<ViewTargeter>(new ViewTargeter()));
    target->AddPreTargetHandler(this);
    view_event_dispatcher_->SetRootViewTarget(target);
  }
  registered_view_id_set_.insert(view->id());
  View::Children::const_iterator it = view->children().begin();
  for (; it != view->children().end(); ++it)
    RegisterSubtree(*it);
}

void WindowManagerApp::UnregisterSubtree(View* view) {
  for (View* child : view->children())
    UnregisterSubtree(child);
  Unregister(view);
}

void WindowManagerApp::Unregister(View* view) {
  RegisteredViewIdSet::iterator it = registered_view_id_set_.find(view->id());
  if (it == registered_view_id_set_.end()) {
    // Because we unregister in OnViewDestroying() we can still get a subsequent
    // OnTreeChanged for the same view. Ignore this one.
    return;
  }
  view->RemoveObserver(this);
  DCHECK(it != registered_view_id_set_.end());
  registered_view_id_set_.erase(it);
}

void WindowManagerApp::DispatchInputEventToView(View* view,
                                                mojo::EventPtr event) {
  window_manager_client_->DispatchInputEventToView(view->id(), event.Pass());
}

void WindowManagerApp::SetViewportSize(const gfx::Size& size) {
  window_manager_client_->SetViewportSize(mojo::Size::From(size));
}

void WindowManagerApp::LaunchViewManager(mojo::ApplicationImpl* app) {
  // TODO(sky): figure out logic if this connection goes away.
  view_manager_client_factory_.reset(
      new mojo::ViewManagerClientFactory(shell_, this));

  ApplicationConnection* view_manager_app =
      app->ConnectToApplication("mojo:view_manager");
  view_manager_app->ConnectToService(&view_manager_service_);

  view_manager_app->AddService<WindowManagerInternal>(this);
  view_manager_app->AddService<mojo::NativeViewportEventDispatcher>(this);

  view_manager_app->ConnectToService(&window_manager_client_);
}

void WindowManagerApp::Create(
    ApplicationConnection* connection,
    mojo::InterfaceRequest<WindowManagerInternal> request) {
  if (wm_internal_binding_.get()) {
    VLOG(1) <<
        "WindowManager allows only one WindowManagerInternal connection.";
    return;
  }
  wm_internal_binding_.reset(
      new mojo::Binding<WindowManagerInternal>(this, request.Pass()));
}

void WindowManagerApp::Create(ApplicationConnection* connection,
                              mojo::InterfaceRequest<WindowManager> request) {
  WindowManagerImpl* wm = new WindowManagerImpl(this, false);
  wm->Bind(request.PassMessagePipe());
  // WindowManagerImpl is deleted when the connection has an error, or from our
  // destructor.
}

void WindowManagerApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::NativeViewportEventDispatcher> request) {
  new NativeViewportEventDispatcherImpl(this, request.Pass());
}

void WindowManagerApp::CreateWindowManagerForViewManagerClient(
    uint16_t connection_id,
    mojo::ScopedMessagePipeHandle window_manager_pipe) {
  // TODO(sky): pass in |connection_id| for validation.
  WindowManagerImpl* wm = new WindowManagerImpl(this, true);
  wm->Bind(window_manager_pipe.Pass());
  // WindowManagerImpl is deleted when the connection has an error, or from our
  // destructor.
}

void WindowManagerApp::SetViewManagerClient(
    mojo::ScopedMessagePipeHandle view_manager_client_request) {
  view_manager_client_.reset(
      mojo::ViewManagerClientFactory::WeakBindViewManagerToPipe(
          mojo::MakeRequest<mojo::ViewManagerClient>(
              view_manager_client_request.Pass()),
          view_manager_service_.Pass(), shell_, this));
}

}  // namespace window_manager
