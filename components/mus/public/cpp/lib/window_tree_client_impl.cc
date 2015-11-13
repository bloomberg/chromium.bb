// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"

#include "base/bind.h"
#include "components/mus/public/cpp/lib/in_flight_change.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace mus {
namespace {

void WindowManagerCallback(mojom::WindowManagerErrorCode error_code) {}

}  // namespace

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local window object from transport data.
Window* AddWindowToConnection(WindowTreeClientImpl* client,
                              Window* parent,
                              const mojom::WindowDataPtr& window_data) {
  // We don't use the cto that takes a WindowTreeConnection here, since it will
  // call back to the service and attempt to create a new window.
  Window* window = WindowPrivate::LocalCreate();
  WindowPrivate private_window(window);
  private_window.set_connection(client);
  private_window.set_id(window_data->window_id);
  private_window.set_visible(window_data->visible);
  private_window.set_drawn(window_data->drawn);
  private_window.LocalSetViewportMetrics(mojom::ViewportMetrics(),
                                         *window_data->viewport_metrics);
  private_window.set_properties(
      window_data->properties
          .To<std::map<std::string, std::vector<uint8_t>>>());
  client->AddWindow(window);
  private_window.LocalSetBounds(gfx::Rect(),
                                window_data->bounds.To<gfx::Rect>());
  if (parent)
    WindowPrivate(parent).LocalAddChild(window);
  return window;
}

Window* BuildWindowTree(WindowTreeClientImpl* client,
                        const mojo::Array<mojom::WindowDataPtr>& windows,
                        Window* initial_parent) {
  std::vector<Window*> parents;
  Window* root = NULL;
  Window* last_window = NULL;
  if (initial_parent)
    parents.push_back(initial_parent);
  for (size_t i = 0; i < windows.size(); ++i) {
    if (last_window && windows[i]->parent_id == last_window->id()) {
      parents.push_back(last_window);
    } else if (!parents.empty()) {
      while (parents.back()->id() != windows[i]->parent_id)
        parents.pop_back();
    }
    Window* window = AddWindowToConnection(
        client, !parents.empty() ? parents.back() : NULL, windows[i]);
    if (!last_window)
      root = window;
    last_window = window;
  }
  return root;
}

WindowTreeConnection* WindowTreeConnection::Create(
    WindowTreeDelegate* delegate,
    mojo::InterfaceRequest<mojom::WindowTreeClient> request,
    CreateType create_type) {
  WindowTreeClientImpl* client =
      new WindowTreeClientImpl(delegate, nullptr, request.Pass());
  if (create_type == CreateType::WAIT_FOR_EMBED)
    client->WaitForEmbed();
  return client;
}

WindowTreeConnection* WindowTreeConnection::CreateForWindowManager(
    WindowTreeDelegate* delegate,
    mojo::InterfaceRequest<mojom::WindowTreeClient> request,
    CreateType create_type,
    WindowManagerDelegate* window_manager_delegate) {
  WindowTreeClientImpl* client = new WindowTreeClientImpl(
      delegate, window_manager_delegate, request.Pass());
  if (create_type == CreateType::WAIT_FOR_EMBED)
    client->WaitForEmbed();
  return client;
}

WindowTreeClientImpl::WindowTreeClientImpl(
    WindowTreeDelegate* delegate,
    WindowManagerDelegate* window_manager_delegate,
    mojo::InterfaceRequest<mojom::WindowTreeClient> request)
    : connection_id_(0),
      next_window_id_(1),
      next_change_id_(1),
      delegate_(delegate),
      window_manager_delegate_(window_manager_delegate),
      root_(nullptr),
      focused_window_(nullptr),
      binding_(this),
      tree_(nullptr),
      is_embed_root_(false),
      in_destructor_(false) {
  // Allow for a null request in tests.
  if (request.is_pending())
    binding_.Bind(request.Pass());
}

WindowTreeClientImpl::~WindowTreeClientImpl() {
  in_destructor_ = true;

  std::vector<Window*> non_owned;
  while (!windows_.empty()) {
    IdToWindowMap::iterator it = windows_.begin();
    if (OwnsWindow(it->second->id())) {
      it->second->Destroy();
    } else {
      non_owned.push_back(it->second);
      windows_.erase(it);
    }
  }

  // Delete the non-owned windows last. In the typical case these are roots. The
  // exception is the window manager and embed roots, which may know about
  // other random windows that it doesn't own.
  // NOTE: we manually delete as we're a friend.
  for (size_t i = 0; i < non_owned.size(); ++i)
    delete non_owned[i];

  delegate_->OnConnectionLost(this);
}

void WindowTreeClientImpl::WaitForEmbed() {
  DCHECK(!root_);
  // OnEmbed() is the first function called.
  binding_.WaitForIncomingMethodCall();
  // TODO(sky): deal with pipe being closed before we get OnEmbed().
}

void WindowTreeClientImpl::DestroyWindow(Id window_id) {
  DCHECK(tree_);
  tree_->DeleteWindow(window_id, ActionCompletedCallback());
}

void WindowTreeClientImpl::AddChild(Id child_id, Id parent_id) {
  DCHECK(tree_);
  tree_->AddWindow(parent_id, child_id, ActionCompletedCallback());
}

void WindowTreeClientImpl::RemoveChild(Id child_id, Id parent_id) {
  DCHECK(tree_);
  tree_->RemoveWindowFromParent(child_id, ActionCompletedCallback());
}

void WindowTreeClientImpl::AddTransientWindow(Window* window,
                                              Id transient_window_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(make_scoped_ptr(
      new CrashInFlightChange(window, ChangeType::ADD_TRANSIENT_WINDOW)));
  tree_->AddTransientWindow(change_id, window->id(), transient_window_id);
}

void WindowTreeClientImpl::RemoveTransientWindowFromParent(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id =
      ScheduleInFlightChange(make_scoped_ptr(new CrashInFlightChange(
          window, ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT)));
  tree_->RemoveTransientWindowFromParent(change_id, window->id());
}

void WindowTreeClientImpl::Reorder(Id window_id,
                                   Id relative_window_id,
                                   mojom::OrderDirection direction) {
  DCHECK(tree_);
  tree_->ReorderWindow(window_id, relative_window_id, direction,
                       ActionCompletedCallback());
}

bool WindowTreeClientImpl::OwnsWindow(Id id) const {
  return HiWord(id) == connection_id_;
}

void WindowTreeClientImpl::SetBounds(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& bounds) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      make_scoped_ptr(new InFlightBoundsChange(window, old_bounds)));
  tree_->SetWindowBounds(change_id, window->id(), mojo::Rect::From(bounds));
}

void WindowTreeClientImpl::SetClientArea(Id window_id,
                                         const gfx::Insets& client_area) {
  DCHECK(tree_);
  tree_->SetClientArea(window_id, mojo::Insets::From(client_area));
}

void WindowTreeClientImpl::SetFocus(Id window_id) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a connection.
  DCHECK(tree_);
  tree_->SetFocus(window_id);
}

void WindowTreeClientImpl::SetVisible(Id window_id, bool visible) {
  DCHECK(tree_);
  tree_->SetWindowVisibility(window_id, visible, ActionCompletedCallback());
}

void WindowTreeClientImpl::SetProperty(Window* window,
                                       const std::string& name,
                                       mojo::Array<uint8_t> data) {
  DCHECK(tree_);

  mojo::Array<uint8_t> old_value;
  if (window->HasSharedProperty(name))
    old_value = mojo::Array<uint8_t>::From(window->properties_[name]);

  const uint32_t change_id = ScheduleInFlightChange(
      make_scoped_ptr(new InFlightPropertyChange(window, name, old_value)));
  tree_->SetWindowProperty(change_id, window->id(), mojo::String(name),
                           data.Pass());
}

void WindowTreeClientImpl::SetWindowTextInputState(
    Id window_id,
    mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetWindowTextInputState(window_id, state.Pass());
}

void WindowTreeClientImpl::SetImeVisibility(Id window_id,
                                            bool visible,
                                            mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetImeVisibility(window_id, visible, state.Pass());
}

void WindowTreeClientImpl::Embed(
    Id window_id,
    mojom::WindowTreeClientPtr client,
    uint32_t policy_bitmask,
    const mojom::WindowTree::EmbedCallback& callback) {
  DCHECK(tree_);
  tree_->Embed(window_id, client.Pass(), policy_bitmask, callback);
}

void WindowTreeClientImpl::RequestSurface(
    Id window_id,
    mojom::SurfaceType type,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {
  DCHECK(tree_);
  tree_->RequestSurface(window_id, type, surface.Pass(), client.Pass());
}

void WindowTreeClientImpl::AddWindow(Window* window) {
  DCHECK(windows_.find(window->id()) == windows_.end());
  windows_[window->id()] = window;
}

void WindowTreeClientImpl::RemoveWindow(Id window_id) {
  if (focused_window_ && focused_window_->id() == window_id)
    OnWindowFocused(0);

  IdToWindowMap::iterator it = windows_.find(window_id);
  if (it != windows_.end())
    windows_.erase(it);

  // Remove any InFlightChanges associated with the window.
  std::set<uint32_t> in_flight_change_ids;
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window()->id() == window_id)
      in_flight_change_ids.insert(pair.first);
  }
  for (auto change_id : in_flight_change_ids)
    in_flight_map_.erase(change_id);
}

void WindowTreeClientImpl::OnRootDestroyed(Window* root) {
  DCHECK_EQ(root, root_);
  root_ = nullptr;

  // When the root is gone we can't do anything useful.
  if (!in_destructor_)
    delete this;
}

void WindowTreeClientImpl::SetPreferredSize(Id window_id,
                                            const gfx::Size& size) {
  tree_->SetPreferredSize(window_id, mojo::Size::From(size),
                          base::Bind(&WindowManagerCallback));
}

void WindowTreeClientImpl::SetShowState(Id window_id,
                                        mojom::ShowState show_state) {
  tree_->SetShowState(window_id, show_state,
                      base::Bind(&WindowManagerCallback));
}

void WindowTreeClientImpl::SetResizeBehavior(
    Id window_id,
    mojom::ResizeBehavior resize_behavior) {
  tree_->SetResizeBehavior(window_id, resize_behavior);
}

InFlightChange* WindowTreeClientImpl::GetOldestInFlightChangeMatching(
    const InFlightChange& change) {
  for (auto& pair : in_flight_map_) {
    if (pair.second->window() == change.window() &&
        pair.second->change_type() == change.change_type() &&
        pair.second->Matches(change)) {
      return pair.second;
    }
  }
  return nullptr;
}

uint32_t WindowTreeClientImpl::ScheduleInFlightChange(
    scoped_ptr<InFlightChange> change) {
  const uint32_t change_id = next_change_id_++;
  in_flight_map_.set(change_id, change.Pass());
  return change_id;
}

bool WindowTreeClientImpl::ApplyServerChangeToExistingInFlightChange(
    const InFlightChange& change) {
  InFlightChange* existing_change = GetOldestInFlightChangeMatching(change);
  if (!existing_change)
    return false;

  existing_change->SetRevertValueFrom(change);
  return true;
}

void WindowTreeClientImpl::OnEmbedImpl(mojom::WindowTree* window_tree,
                                       ConnectionSpecificId connection_id,
                                       mojom::WindowDataPtr root_data,
                                       Id focused_window_id,
                                       uint32 access_policy) {
  tree_ = window_tree;
  connection_id_ = connection_id;
  is_embed_root_ =
      (access_policy & mojom::WindowTree::ACCESS_POLICY_EMBED_ROOT) != 0;

  DCHECK(!root_);
  root_ = AddWindowToConnection(this, nullptr, root_data);

  focused_window_ = GetWindowById(focused_window_id);

  delegate_->OnEmbed(root_);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClientImpl, WindowTreeConnection implementation:

Window* WindowTreeClientImpl::GetRoot() {
  return root_;
}

Window* WindowTreeClientImpl::GetWindowById(Id id) {
  IdToWindowMap::const_iterator it = windows_.find(id);
  return it != windows_.end() ? it->second : NULL;
}

Window* WindowTreeClientImpl::GetFocusedWindow() {
  return focused_window_;
}

Window* WindowTreeClientImpl::NewWindow() {
  DCHECK(tree_);
  Window* window =
      new Window(this, MakeTransportId(connection_id_, next_window_id_++));
  AddWindow(window);

  const uint32_t change_id = ScheduleInFlightChange(
      make_scoped_ptr(new CrashInFlightChange(window, ChangeType::NEW_WINDOW)));
  tree_->NewWindow(change_id, window->id());
  return window;
}

bool WindowTreeClientImpl::IsEmbedRoot() {
  return is_embed_root_;
}

ConnectionSpecificId WindowTreeClientImpl::GetConnectionId() {
  return connection_id_;
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClientImpl, WindowTreeClient implementation:

void WindowTreeClientImpl::OnEmbed(ConnectionSpecificId connection_id,
                                   mojom::WindowDataPtr root_data,
                                   mojom::WindowTreePtr tree,
                                   Id focused_window_id,
                                   uint32 access_policy) {
  DCHECK(!tree_ptr_);
  tree_ptr_ = tree.Pass();
  tree_ptr_.set_connection_error_handler([this]() { delete this; });
  OnEmbedImpl(tree_ptr_.get(), connection_id, root_data.Pass(),
              focused_window_id, access_policy);
}

void WindowTreeClientImpl::OnEmbeddedAppDisconnected(Id window_id) {
  Window* window = GetWindowById(window_id);
  if (window) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window).observers(),
                      OnWindowEmbeddedAppDisconnected(window));
  }
}

void WindowTreeClientImpl::OnUnembed() {
  delegate_->OnUnembed();
  // This will send out the various notifications.
  delete this;
}

void WindowTreeClientImpl::OnWindowBoundsChanged(Id window_id,
                                                 mojo::RectPtr old_bounds,
                                                 mojo::RectPtr new_bounds) {
  Window* window = GetWindowById(window_id);
  if (!window)
    return;

  InFlightBoundsChange new_change(window, new_bounds.To<gfx::Rect>());
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window)
      .LocalSetBounds(old_bounds.To<gfx::Rect>(), new_bounds.To<gfx::Rect>());
}

void WindowTreeClientImpl::OnClientAreaChanged(
    uint32_t window_id,
    mojo::InsetsPtr old_client_area,
    mojo::InsetsPtr new_client_area) {
  Window* window = GetWindowById(window_id);
  if (window)
    WindowPrivate(window).LocalSetClientArea(new_client_area.To<gfx::Insets>());
}

void WindowTreeClientImpl::OnTransientWindowAdded(
    uint32_t window_id,
    uint32_t transient_window_id) {
  Window* window = GetWindowById(window_id);
  Window* transient_window = GetWindowById(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight add from the server.
  if (window && transient_window)
    WindowPrivate(window).LocalAddTransientWindow(transient_window);
}

void WindowTreeClientImpl::OnTransientWindowRemoved(
    uint32_t window_id,
    uint32_t transient_window_id) {
  Window* window = GetWindowById(window_id);
  Window* transient_window = GetWindowById(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight delete from the server.
  if (window && transient_window)
    WindowPrivate(window).LocalRemoveTransientWindow(transient_window);
}

namespace {

void SetViewportMetricsOnDecendants(Window* root,
                                    const mojom::ViewportMetrics& old_metrics,
                                    const mojom::ViewportMetrics& new_metrics) {
  WindowPrivate(root).LocalSetViewportMetrics(old_metrics, new_metrics);
  const Window::Children& children = root->children();
  for (size_t i = 0; i < children.size(); ++i)
    SetViewportMetricsOnDecendants(children[i], old_metrics, new_metrics);
}
}

void WindowTreeClientImpl::OnWindowViewportMetricsChanged(
    mojom::ViewportMetricsPtr old_metrics,
    mojom::ViewportMetricsPtr new_metrics) {
  Window* window = GetRoot();
  if (window)
    SetViewportMetricsOnDecendants(window, *old_metrics, *new_metrics);
}

void WindowTreeClientImpl::OnWindowHierarchyChanged(
    Id window_id,
    Id new_parent_id,
    Id old_parent_id,
    mojo::Array<mojom::WindowDataPtr> windows) {
  Window* initial_parent =
      windows.size() ? GetWindowById(windows[0]->parent_id) : NULL;

  const bool was_window_known = GetWindowById(window_id) != nullptr;

  BuildWindowTree(this, windows, initial_parent);

  // If the window was not known, then BuildWindowTree() will have created it
  // and parented the window.
  if (!was_window_known)
    return;

  Window* new_parent = GetWindowById(new_parent_id);
  Window* old_parent = GetWindowById(old_parent_id);
  Window* window = GetWindowById(window_id);
  if (new_parent)
    WindowPrivate(new_parent).LocalAddChild(window);
  else
    WindowPrivate(old_parent).LocalRemoveChild(window);
}

void WindowTreeClientImpl::OnWindowReordered(Id window_id,
                                             Id relative_window_id,
                                             mojom::OrderDirection direction) {
  Window* window = GetWindowById(window_id);
  Window* relative_window = GetWindowById(relative_window_id);
  if (window && relative_window)
    WindowPrivate(window).LocalReorder(relative_window, direction);
}

void WindowTreeClientImpl::OnWindowDeleted(Id window_id) {
  Window* window = GetWindowById(window_id);
  if (window)
    WindowPrivate(window).LocalDestroy();
}

void WindowTreeClientImpl::OnWindowVisibilityChanged(Id window_id,
                                                     bool visible) {
  // TODO(sky): there is a race condition here. If this client and another
  // client change the visibility at the same time the wrong value may be set.
  // Deal with this some how.
  Window* window = GetWindowById(window_id);
  if (window)
    WindowPrivate(window).LocalSetVisible(visible);
}

void WindowTreeClientImpl::OnWindowDrawnStateChanged(Id window_id, bool drawn) {
  Window* window = GetWindowById(window_id);
  if (window)
    WindowPrivate(window).LocalSetDrawn(drawn);
}

void WindowTreeClientImpl::OnWindowSharedPropertyChanged(
    Id window_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  Window* window = GetWindowById(window_id);
  if (!window)
    return;

  InFlightPropertyChange new_change(window, name, new_data);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetSharedProperty(name, new_data.Pass());
}

void WindowTreeClientImpl::OnWindowInputEvent(
    Id window_id,
    mojom::EventPtr event,
    const mojo::Callback<void()>& ack_callback) {
  Window* window = GetWindowById(window_id);
  if (window) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window).observers(),
                      OnWindowInputEvent(window, event));
  }
  ack_callback.Run();
}

void WindowTreeClientImpl::OnWindowFocused(Id focused_window_id) {
  Window* focused = GetWindowById(focused_window_id);
  Window* blurred = focused_window_;
  // Update |focused_window_| before calling any of the observers, so that the
  // observers get the correct result from calling |Window::HasFocus()|,
  // |WindowTreeConnection::GetFocusedWindow()| etc.
  focused_window_ = focused;
  if (blurred) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(blurred).observers(),
                      OnWindowFocusChanged(focused, blurred));
  }
  if (focused) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(focused).observers(),
                      OnWindowFocusChanged(focused, blurred));
  }
}

void WindowTreeClientImpl::OnChangeCompleted(uint32 change_id, bool success) {
  scoped_ptr<InFlightChange> change(in_flight_map_.take_and_erase(change_id));
  if (!change)
    return;

  if (!success)
    change->ChangeFailed();

  InFlightChange* next_change = GetOldestInFlightChangeMatching(*change);
  if (next_change) {
    if (!success)
      next_change->SetRevertValueFrom(*change);
  } else if (!success) {
    change->Revert();
  }
}

void WindowTreeClientImpl::WmSetBounds(uint32_t change_id,
                                       Id window_id,
                                       mojo::RectPtr transit_bounds) {
  Window* window = GetWindowById(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    gfx::Rect bounds = transit_bounds.To<gfx::Rect>();
    result = window_manager_delegate_->OnWmSetBounds(window, &bounds);
    if (result) {
      // If the resulting bounds differ return false. Returning false ensures
      // the client applies the bounds we set below.
      result = bounds == transit_bounds.To<gfx::Rect>();
      window->SetBounds(bounds);
    }
  }
  tree_->WmResponse(change_id, result);
}

void WindowTreeClientImpl::WmSetProperty(uint32_t change_id,
                                         Id window_id,
                                         const mojo::String& name,
                                         mojo::Array<uint8_t> transit_data) {
  Window* window = GetWindowById(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    scoped_ptr<std::vector<uint8_t>> data;
    if (!transit_data.is_null()) {
      data.reset(
          new std::vector<uint8_t>(transit_data.To<std::vector<uint8_t>>()));
    }
    result = window_manager_delegate_->OnWmSetProperty(window, name, &data);
    if (result) {
      // If the resulting bounds differ return false. Returning false ensures
      // the client applies the bounds we set below.
      window->SetSharedPropertyInternal(name, data.get());
    }
  }
  tree_->WmResponse(change_id, result);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClientImpl, private:

void WindowTreeClientImpl::OnActionCompleted(bool success) {
  if (!change_acked_callback_.is_null())
    change_acked_callback_.Run();
}

mojo::Callback<void(bool)> WindowTreeClientImpl::ActionCompletedCallback() {
  return [this](bool success) { OnActionCompleted(success); };
}

}  // namespace mus
