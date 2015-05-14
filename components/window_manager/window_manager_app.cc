// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/window_manager/window_manager_app.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/window_manager/window_manager_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

using mojo::ApplicationConnection;
using mojo::Id;
using mojo::ServiceProvider;
using mojo::View;
using mojo::WindowManager;

namespace window_manager {

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
  if (root_)
    root_->RemoveObserver(this);

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

bool WindowManagerApp::IsReady() const {
  return !!root_;
}

void WindowManagerApp::AddAccelerator(mojo::KeyboardCode keyboard_code,
                                      mojo::EventFlags flags) {
  window_manager_client_->AddAccelerator(keyboard_code, flags);
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

  root_->AddObserver(this);

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

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewObserver implementation:

void WindowManagerApp::OnViewDestroying(View* view) {
  DCHECK_EQ(root_, view);
  root_->RemoveObserver(this);
  root_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, private:

void WindowManagerApp::DispatchInputEventToViewDEPRECATED(
    View* view,
    mojo::EventPtr event) {
  window_manager_client_->DispatchInputEventToViewDEPRECATED(view->id(),
                                                             event.Pass());
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

void WindowManagerApp::SetViewManagerClient(
    mojo::ScopedMessagePipeHandle view_manager_client_request) {
  view_manager_client_.reset(
      mojo::ViewManagerClientFactory::WeakBindViewManagerToPipe(
          mojo::MakeRequest<mojo::ViewManagerClient>(
              view_manager_client_request.Pass()),
          view_manager_service_.Pass(), shell_, this));
}

void WindowManagerApp::OnAccelerator(mojo::EventPtr event) {
  window_manager_delegate_->OnAcceleratorPressed(
      root_->view_manager()->GetFocusedView(),
      event->key_data->windows_key_code, event->flags);
}

}  // namespace window_manager
