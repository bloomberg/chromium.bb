// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
#define SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/services/window_manager/capture_controller_observer.h"
#include "mojo/services/window_manager/focus_controller_observer.h"
#include "mojo/services/window_manager/native_viewport_event_dispatcher_impl.h"
#include "mojo/services/window_manager/view_target.h"
#include "mojo/services/window_manager/window_manager_impl.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/types.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_client_factory.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_delegate.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_observer.h"
#include "third_party/mojo_services/src/window_manager/public/interfaces/window_manager_internal.mojom.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/event_handler.h"

namespace gfx {
class Size;
}

namespace window_manager {

class CaptureController;
class FocusController;
class FocusRules;
class ViewEventDispatcher;
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
      public ui::EventHandler,
      public FocusControllerObserver,
      public CaptureControllerObserver,
      public mojo::InterfaceFactory<mojo::WindowManager>,
      public mojo::InterfaceFactory<mojo::WindowManagerInternal>,
      public mojo::InterfaceFactory<mojo::NativeViewportEventDispatcher>,
      public mojo::WindowManagerInternal {
 public:
  WindowManagerApp(ViewManagerDelegate* view_manager_delegate,
                   WindowManagerDelegate* window_manager_delegate);
  ~WindowManagerApp() override;

  ViewEventDispatcher* event_dispatcher() {
    return view_event_dispatcher_.get();
  }

  // Register/deregister new connections to the window manager service.
  void AddConnection(WindowManagerImpl* connection);
  void RemoveConnection(WindowManagerImpl* connection);

  // These are canonical implementations of the window manager API methods.
  bool SetCapture(mojo::Id view);
  bool FocusWindow(mojo::Id view);
  bool ActivateWindow(mojo::Id view);

  void DispatchInputEventToView(mojo::View* view, mojo::EventPtr event);
  void SetViewportSize(const gfx::Size& size);

  bool IsReady() const;

  FocusController* focus_controller() { return focus_controller_.get(); }
  CaptureController* capture_controller() { return capture_controller_.get(); }

  void InitFocus(scoped_ptr<FocusRules> rules);

  ui::AcceleratorManager* accelerator_manager() {
    return &accelerator_manager_;
  }

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
  typedef std::set<mojo::Id> RegisteredViewIdSet;

  struct PendingEmbed;
  class WindowManagerInternalImpl;

  mojo::ViewManager* view_manager() {
    return root_ ? root_->view_manager() : nullptr;
  }

  bool SetCaptureImpl(mojo::View* view);
  bool FocusWindowImpl(mojo::View* view);
  bool ActivateWindowImpl(mojo::View* view);

  ui::Accelerator ConvertEventToAccelerator(const ui::KeyEvent* event);

  // Creates an ViewTarget for every view in the hierarchy beneath |view|,
  // and adds to the registry so that it can be retrieved later via
  // GetViewTargetForViewId().
  // TODO(beng): perhaps View should have a property bag.
  void RegisterSubtree(mojo::View* view);

  // Recursively invokes Unregister() for |view| and all its descendants.
  void UnregisterSubtree(mojo::View* view);

  // Deletes the ViewTarget associated with the hierarchy beneath |id|,
  // and removes from the registry.
  void Unregister(mojo::View* view);

  // Overridden from ViewManagerDelegate:
  void OnEmbed(mojo::View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override;
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override;
  bool OnPerformAction(mojo::View* view, const std::string& action) override;

  // Overridden from ViewObserver:
  void OnTreeChanged(const ViewObserver::TreeChangeParams& params) override;
  void OnViewDestroying(mojo::View* view) override;

  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  // Overridden from mojo::FocusControllerObserver:
  void OnFocused(mojo::View* gained_focus) override;
  void OnActivated(mojo::View* gained_active) override;

  // Overridden from mojo::CaptureControllerObserver:
  void OnCaptureChanged(mojo::View* gained_capture) override;

  // Creates the connection to the ViewManager.
  void LaunchViewManager(mojo::ApplicationImpl* app);

  // InterfaceFactory<WindowManagerInternal>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::WindowManagerInternal> request) override;

  // InterfaceFactory<WindowManager>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::WindowManager> request) override;

  // InterfaceFactory<NativeViewportEventDispatcher>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NativeViewportEventDispatcher>
                  request) override;

  // WindowManagerInternal:
  void CreateWindowManagerForViewManagerClient(
      uint16_t connection_id,
      mojo::ScopedMessagePipeHandle window_manager_pipe) override;
  void SetViewManagerClient(
      mojo::ScopedMessagePipeHandle view_manager_client_request) override;

  mojo::Shell* shell_;

  ViewManagerDelegate* wrapped_view_manager_delegate_;
  WindowManagerDelegate* window_manager_delegate_;

  mojo::ViewManagerServicePtr view_manager_service_;
  scoped_ptr<mojo::ViewManagerClientFactory> view_manager_client_factory_;
  mojo::View* root_;

  scoped_ptr<FocusController> focus_controller_;
  scoped_ptr<CaptureController> capture_controller_;

  ui::AcceleratorManager accelerator_manager_;

  Connections connections_;
  RegisteredViewIdSet registered_view_id_set_;

  mojo::WindowManagerInternalClientPtr window_manager_client_;

  ScopedVector<PendingEmbed> pending_embeds_;

  scoped_ptr<mojo::ViewManagerClient> view_manager_client_;

  scoped_ptr<ViewEventDispatcher> view_event_dispatcher_;

  scoped_ptr<mojo::Binding<WindowManagerInternal>> wm_internal_binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApp);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
