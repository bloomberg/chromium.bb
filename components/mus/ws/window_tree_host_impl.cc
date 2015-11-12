// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_host_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/focus_controller.h"
#include "components/mus/ws/window_tree_host_delegate.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace mus {
namespace ws {

WindowTreeHostImpl::WindowTreeHostImpl(
    mojom::WindowTreeHostClientPtr client,
    ConnectionManager* connection_manager,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state,
    mojom::WindowManagerPtr window_manager)
    : delegate_(nullptr),
      connection_manager_(connection_manager),
      client_(client.Pass()),
      event_dispatcher_(this),
      display_manager_(
          DisplayManager::Create(app_impl, gpu_state, surfaces_state)),
      focus_controller_(new FocusController(this)),
      window_manager_(window_manager.Pass()) {
  display_manager_->Init(this);
  if (client_) {
    client_.set_connection_error_handler(base::Bind(
        &WindowTreeHostImpl::OnClientClosed, base::Unretained(this)));
  }
}

WindowTreeHostImpl::~WindowTreeHostImpl() {
  for (ServerWindow* window : windows_needing_frame_destruction_)
    window->RemoveObserver(this);
}

void WindowTreeHostImpl::Init(WindowTreeHostDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_ && root_)
    delegate_->OnDisplayInitialized();
}

WindowTreeImpl* WindowTreeHostImpl::GetWindowTree() {
  return delegate_ ? delegate_->GetWindowTree() : nullptr;
}

bool WindowTreeHostImpl::IsWindowAttachedToRoot(
    const ServerWindow* window) const {
  return root_->Contains(window) && window != root_.get();
}

bool WindowTreeHostImpl::SchedulePaintIfInViewport(const ServerWindow* window,
                                                   const gfx::Rect& bounds) {
  if (root_->Contains(window)) {
    display_manager_->SchedulePaint(window, bounds);
    return true;
  }
  return false;
}

void WindowTreeHostImpl::ScheduleSurfaceDestruction(ServerWindow* window) {
  if (!display_manager_->IsFramePending()) {
    window->DestroySurfacesScheduledForDestruction();
    return;
  }
  if (windows_needing_frame_destruction_.count(window))
    return;
  windows_needing_frame_destruction_.insert(window);
  window->AddObserver(this);
}

const mojom::ViewportMetrics& WindowTreeHostImpl::GetViewportMetrics() const {
  return display_manager_->GetViewportMetrics();
}

void WindowTreeHostImpl::SetFocusedWindow(ServerWindow* new_focused_window) {
  ServerWindow* old_focused_window = focus_controller_->GetFocusedWindow();
  if (old_focused_window == new_focused_window)
    return;
  DCHECK(root_window()->Contains(new_focused_window));
  focus_controller_->SetFocusedWindow(new_focused_window);
  // TODO(beng): have the FocusController notify us via FocusControllerDelegate.
  OnFocusChanged(old_focused_window, new_focused_window);
}

ServerWindow* WindowTreeHostImpl::GetFocusedWindow() {
  return focus_controller_->GetFocusedWindow();
}

void WindowTreeHostImpl::DestroyFocusController() {
  focus_controller_.reset();
}

void WindowTreeHostImpl::UpdateTextInputState(ServerWindow* window,
                                              const ui::TextInputState& state) {
  // Do not need to update text input for unfocused windows.
  if (!display_manager_ || focus_controller_->GetFocusedWindow() != window)
    return;
  display_manager_->UpdateTextInputState(state);
}

void WindowTreeHostImpl::SetImeVisibility(ServerWindow* window, bool visible) {
  // Do not need to show or hide IME for unfocused window.
  if (focus_controller_->GetFocusedWindow() != window)
    return;
  display_manager_->SetImeVisibility(visible);
}

void WindowTreeHostImpl::SetSize(mojo::SizePtr size) {
  display_manager_->SetViewportSize(size.To<gfx::Size>());
}

void WindowTreeHostImpl::SetTitle(const mojo::String& title) {
  display_manager_->SetTitle(title.To<base::string16>());
}

void WindowTreeHostImpl::AddAccelerator(uint32_t id,
                                        mojom::EventMatcherPtr event_matcher) {
  event_dispatcher_.AddAccelerator(id, event_matcher.Pass());
}

void WindowTreeHostImpl::RemoveAccelerator(uint32_t id) {
  event_dispatcher_.RemoveAccelerator(id);
}

void WindowTreeHostImpl::OnClientClosed() {
  // |display_manager_.reset()| destroys the display-manager first, and then
  // sets |display_manager_| to nullptr. However, destroying |display_manager_|
  // can destroy the corresponding WindowTreeHostConnection, and |this|. So
  // setting it to nullptr afterwards in reset() ends up writing on free'd
  // memory. So transfer over to a local scoped_ptr<> before destroying it.
  scoped_ptr<DisplayManager> temp = display_manager_.Pass();
}

ServerWindow* WindowTreeHostImpl::GetRootWindow() {
  return root_.get();
}

void WindowTreeHostImpl::OnEvent(mojom::EventPtr event) {
  event_dispatcher_.OnEvent(event.Pass());
}

void WindowTreeHostImpl::OnDisplayClosed() {
  if (delegate_)
    delegate_->OnDisplayClosed();
}

void WindowTreeHostImpl::OnViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerWindow(
        RootWindowId(connection_manager_->GetAndAdvanceNextHostId())));
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    root_->SetVisible(true);
    if (delegate_)
      delegate_->OnDisplayInitialized();
    event_dispatcher_.set_root(root_.get());
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
  }
  connection_manager_->ProcessViewportMetricsChanged(old_metrics, new_metrics);
}

void WindowTreeHostImpl::OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) {
  event_dispatcher_.set_surface_id(surface_id);
}

void WindowTreeHostImpl::OnCompositorFrameDrawn() {
  std::set<ServerWindow*> windows;
  windows.swap(windows_needing_frame_destruction_);
  for (ServerWindow* window : windows) {
    window->RemoveObserver(this);
    window->DestroySurfacesScheduledForDestruction();
  }
}

void WindowTreeHostImpl::OnFocusChanged(ServerWindow* old_focused_window,
                                        ServerWindow* new_focused_window) {
  // There are up to four connections that need to be notified:
  // . the connection containing |old_focused_window|.
  // . the connection with |old_focused_window| as its root.
  // . the connection containing |new_focused_window|.
  // . the connection with |new_focused_window| as its root.
  // Some of these connections may be the same. The following takes care to
  // notify each only once.
  WindowTreeImpl* owning_connection_old = nullptr;
  WindowTreeImpl* embedded_connection_old = nullptr;

  if (old_focused_window) {
    owning_connection_old = connection_manager_->GetConnection(
        old_focused_window->id().connection_id);
    if (owning_connection_old) {
      owning_connection_old->ProcessFocusChanged(old_focused_window,
                                                 new_focused_window);
    }
    embedded_connection_old =
        connection_manager_->GetConnectionWithRoot(old_focused_window->id());
    if (embedded_connection_old) {
      DCHECK_NE(owning_connection_old, embedded_connection_old);
      embedded_connection_old->ProcessFocusChanged(old_focused_window,
                                                   new_focused_window);
    }
  }
  WindowTreeImpl* owning_connection_new = nullptr;
  WindowTreeImpl* embedded_connection_new = nullptr;
  if (new_focused_window) {
    owning_connection_new = connection_manager_->GetConnection(
        new_focused_window->id().connection_id);
    if (owning_connection_new &&
        owning_connection_new != owning_connection_old &&
        owning_connection_new != embedded_connection_old) {
      owning_connection_new->ProcessFocusChanged(old_focused_window,
                                                 new_focused_window);
    }
    embedded_connection_new =
        connection_manager_->GetConnectionWithRoot(new_focused_window->id());
    if (embedded_connection_new &&
        embedded_connection_new != owning_connection_old &&
        embedded_connection_new != embedded_connection_old) {
      DCHECK_NE(owning_connection_new, embedded_connection_new);
      embedded_connection_new->ProcessFocusChanged(old_focused_window,
                                                   new_focused_window);
    }
  }

  // Ensure that we always notify the root connection of a focus change.
  WindowTreeImpl* root_tree = GetWindowTree();
  if (root_tree != owning_connection_old &&
      root_tree != embedded_connection_old &&
      root_tree != owning_connection_new &&
      root_tree != embedded_connection_new) {
    root_tree->ProcessFocusChanged(old_focused_window, new_focused_window);
  }

  UpdateTextInputState(new_focused_window,
                       new_focused_window->text_input_state());
}

void WindowTreeHostImpl::OnAccelerator(uint32_t accelerator_id,
                                       mojom::EventPtr event) {
  client()->OnAccelerator(accelerator_id, event.Pass());
}

void WindowTreeHostImpl::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowTreeHostImpl::GetFocusedWindowForEventDispatcher() {
  return GetFocusedWindow();
}

void WindowTreeHostImpl::DispatchInputEventToWindow(ServerWindow* target,
                                                    bool in_nonclient_area,
                                                    mojom::EventPtr event) {
  // If the event is in the non-client area the event goes to the owner of
  // the window. Otherwise if the window is an embed root, forward to the
  // embedded window.
  WindowTreeImpl* connection =
      in_nonclient_area
          ? connection_manager_->GetConnection(target->id().connection_id)
          : connection_manager_->GetConnectionWithRoot(target->id());
  if (!connection) {
    DCHECK(!in_nonclient_area);
    connection = connection_manager_->GetConnection(target->id().connection_id);
  }
  connection->client()->OnWindowInputEvent(WindowIdToTransportId(target->id()),
                                           event.Pass(),
                                           base::Bind(&base::DoNothing));
}

void WindowTreeHostImpl::OnWindowDestroyed(ServerWindow* window) {
  windows_needing_frame_destruction_.erase(window);
  window->RemoveObserver(this);
}

}  // namespace ws
}  // namespace mus
