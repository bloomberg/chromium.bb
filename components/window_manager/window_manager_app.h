// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
#define COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "components/window_manager/public/interfaces/window_manager_internal.mojom.h"
#include "components/window_manager/window_manager_impl.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"

namespace gfx {
class Size;
}

namespace window_manager {

class WindowManagerDelegate;
class WindowManagerImpl;

// Implements core window manager functionality that could conceivably be shared
// across multiple window managers implementing superficially different user
// experiences. Establishes communication with the view manager.
// A window manager wishing to use this core should create and own an instance
// of this object. They may implement the associated ViewManager/WindowManager
// delegate interfaces exposed by the view manager, this object provides the
// canonical implementation of said interfaces but will call out to the wrapped
// instances.
class WindowManagerApp
    : public mojo::ApplicationDelegate,
      public mojo::ViewManagerDelegate,
      public mojo::ViewObserver,
      public mojo::InterfaceFactory<mojo::WindowManager>,
      public mojo::InterfaceFactory<mojo::WindowManagerInternal>,
      public mojo::WindowManagerInternal {
 public:
  WindowManagerApp(ViewManagerDelegate* view_manager_delegate,
                   WindowManagerDelegate* window_manager_delegate);
  ~WindowManagerApp() override;

  // Register/deregister new connections to the window manager service.
  void AddConnection(WindowManagerImpl* connection);
  void RemoveConnection(WindowManagerImpl* connection);

  void DispatchInputEventToViewDEPRECATED(mojo::View* view,
                                          mojo::EventPtr event);
  void SetViewportSize(const gfx::Size& size);

  bool IsReady() const;

  void AddAccelerator(mojo::KeyboardCode keyboard_code, mojo::EventFlags flags);
  void RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                         mojo::EventFlags flags);

  // WindowManagerImpl::Embed() forwards to this. If connected to ViewManager
  // then forwards to delegate, otherwise waits for connection to establish then
  // forwards.
  void Embed(const mojo::String& url,
             mojo::InterfaceRequest<mojo::ServiceProvider> services,
             mojo::ServiceProviderPtr exposed_services);

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* impl) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

 private:
  // TODO(sky): rename this. Connections is ambiguous.
  typedef std::set<WindowManagerImpl*> Connections;

  struct PendingEmbed;

  mojo::ViewManager* view_manager() {
    return root_ ? root_->view_manager() : nullptr;
  }

  // Overridden from ViewManagerDelegate:
  void OnEmbed(mojo::View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override;
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override;

  // Overridden from ViewObserver:
  void OnViewDestroying(mojo::View* view) override;

  // Creates the connection to the ViewManager.
  void LaunchViewManager(mojo::ApplicationImpl* app);

  // InterfaceFactory<WindowManagerInternal>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::WindowManagerInternal> request) override;

  // InterfaceFactory<WindowManager>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::WindowManager> request) override;

  // WindowManagerInternal:
  void SetViewManagerClient(
      mojo::ScopedMessagePipeHandle view_manager_client_request) override;
  void OnAccelerator(mojo::EventPtr event) override;

  mojo::Shell* shell_;

  ViewManagerDelegate* wrapped_view_manager_delegate_;
  WindowManagerDelegate* window_manager_delegate_;

  mojo::ViewManagerServicePtr view_manager_service_;
  scoped_ptr<mojo::ViewManagerClientFactory> view_manager_client_factory_;

  mojo::View* root_;

  Connections connections_;

  mojo::WindowManagerInternalClientPtr window_manager_client_;

  ScopedVector<PendingEmbed> pending_embeds_;

  scoped_ptr<mojo::ViewManagerClient> view_manager_client_;

  scoped_ptr<mojo::Binding<WindowManagerInternal>> wm_internal_binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApp);
};

}  // namespace window_manager

#endif  // COMPONENTS_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
