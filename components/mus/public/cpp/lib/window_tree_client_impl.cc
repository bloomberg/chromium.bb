// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/lib/in_flight_change.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tracker.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_connection_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace mus {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

Id server_id(Window* window) {
  return WindowPrivate(window).server_id();
}

// Helper called to construct a local window object from transport data.
Window* AddWindowToConnection(WindowTreeClientImpl* client,
                              Window* parent,
                              const mojom::WindowDataPtr& window_data) {
  // We don't use the ctor that takes a WindowTreeConnection here, since it
  // will call back to the service and attempt to create a new window.
  Window* window = WindowPrivate::LocalCreate();
  WindowPrivate private_window(window);
  private_window.set_connection(client);
  private_window.set_server_id(window_data->window_id);
  private_window.set_visible(window_data->visible);
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
    if (last_window && windows[i]->parent_id == server_id(last_window)) {
      parents.push_back(last_window);
    } else if (!parents.empty()) {
      while (server_id(parents.back()) != windows[i]->parent_id)
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
    shell::Connector* connector) {
  WindowTreeClientImpl* client =
      new WindowTreeClientImpl(delegate, nullptr, nullptr);
  client->ConnectViaWindowTreeFactory(connector);
  return client;
}

WindowTreeConnection* WindowTreeConnection::Create(
    WindowTreeDelegate* delegate,
    mojo::InterfaceRequest<mojom::WindowTreeClient> request,
    CreateType create_type) {
  WindowTreeClientImpl* client =
      new WindowTreeClientImpl(delegate, nullptr, std::move(request));
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
      delegate, window_manager_delegate, std::move(request));
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
      capture_window_(nullptr),
      focused_window_(nullptr),
      binding_(this),
      tree_(nullptr),
      delete_on_no_roots_(true),
      in_destructor_(false) {
  // Allow for a null request in tests.
  if (request.is_pending())
    binding_.Bind(std::move(request));
  if (window_manager_delegate)
    window_manager_delegate->SetWindowManagerClient(this);
}

WindowTreeClientImpl::~WindowTreeClientImpl() {
  in_destructor_ = true;

  std::vector<Window*> non_owned;
  WindowTracker tracker;
  while (!windows_.empty()) {
    IdToWindowMap::iterator it = windows_.begin();
    if (OwnsWindow(it->second)) {
      it->second->Destroy();
    } else {
      tracker.Add(it->second);
      windows_.erase(it);
    }
  }

  // Delete the non-owned windows last. In the typical case these are roots. The
  // exception is the window manager and embed roots, which may know about
  // other random windows that it doesn't own.
  // NOTE: we manually delete as we're a friend.
  while (!tracker.windows().empty())
    delete tracker.windows().front();

  delegate_->OnConnectionLost(this);
}

void WindowTreeClientImpl::ConnectViaWindowTreeFactory(
    shell::Connector* connector) {
  // Clients created with no root shouldn't delete automatically.
  delete_on_no_roots_ = false;

  // The connection id doesn't really matter, we use 101 purely for debugging.
  connection_id_ = 101;

  mojom::WindowTreeFactoryPtr factory;
  connector->ConnectToInterface("mojo:mus", &factory);
  factory->CreateWindowTree(GetProxy(&tree_ptr_),
                            binding_.CreateInterfacePtrAndBind());
  tree_ = tree_ptr_.get();
}

void WindowTreeClientImpl::WaitForEmbed() {
  DCHECK(roots_.empty());
  // OnEmbed() is the first function called.
  binding_.WaitForIncomingMethodCall();
  // TODO(sky): deal with pipe being closed before we get OnEmbed().
}

void WindowTreeClientImpl::DestroyWindow(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(window, ChangeType::DELETE_WINDOW)));
  tree_->DeleteWindow(change_id, server_id(window));
}

void WindowTreeClientImpl::AddChild(Window* parent, Id child_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new CrashInFlightChange(parent, ChangeType::ADD_CHILD)));
  tree_->AddWindow(change_id, parent->server_id(), child_id);
}

void WindowTreeClientImpl::RemoveChild(Window* parent, Id child_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(parent, ChangeType::REMOVE_CHILD)));
  tree_->RemoveWindowFromParent(change_id, child_id);
}

void WindowTreeClientImpl::AddTransientWindow(Window* window,
                                              Id transient_window_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(window, ChangeType::ADD_TRANSIENT_WINDOW)));
  tree_->AddTransientWindow(change_id, server_id(window), transient_window_id);
}

void WindowTreeClientImpl::RemoveTransientWindowFromParent(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id =
      ScheduleInFlightChange(base::WrapUnique(new CrashInFlightChange(
          window, ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT)));
  tree_->RemoveTransientWindowFromParent(change_id, server_id(window));
}

void WindowTreeClientImpl::SetModal(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightSetModalChange(window)));
  tree_->SetModal(change_id, server_id(window));
}

void WindowTreeClientImpl::Reorder(Window* window,
                                   Id relative_window_id,
                                   mojom::OrderDirection direction) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new CrashInFlightChange(window, ChangeType::REORDER)));
  tree_->ReorderWindow(change_id, server_id(window), relative_window_id,
                       direction);
}

bool WindowTreeClientImpl::OwnsWindow(Window* window) const {
  // Windows created via CreateTopLevelWindow() are not owned by us, but have
  // our connection id.
  return HiWord(server_id(window)) == connection_id_ &&
         roots_.count(window) == 0;
}

void WindowTreeClientImpl::SetBounds(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& bounds) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightBoundsChange(window, old_bounds)));
  tree_->SetWindowBounds(change_id, server_id(window),
                         mojo::Rect::From(bounds));
}

void WindowTreeClientImpl::SetCapture(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a connection.
  DCHECK(tree_);
  if (capture_window_ == window)
    return;
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightCaptureChange(this, capture_window_)));
  tree_->SetCapture(change_id, server_id(window));
  LocalSetCapture(window);
}

void WindowTreeClientImpl::ReleaseCapture(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a connection.
  DCHECK(tree_);
  if (capture_window_ != window)
    return;
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightCaptureChange(this, window)));
  tree_->ReleaseCapture(change_id, server_id(window));
  LocalSetCapture(nullptr);
}

void WindowTreeClientImpl::SetClientArea(
    Id window_id,
    const gfx::Insets& client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  DCHECK(tree_);
  tree_->SetClientArea(
      window_id, mojo::Insets::From(client_area),
      mojo::Array<mojo::RectPtr>::From(additional_client_areas));
}

void WindowTreeClientImpl::SetFocus(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a connection.
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightFocusChange(this, focused_window_)));
  tree_->SetFocus(change_id, window ? server_id(window) : 0);
  LocalSetFocus(window);
}

void WindowTreeClientImpl::SetCanFocus(Id window_id, bool can_focus) {
  DCHECK(tree_);
  tree_->SetCanFocus(window_id, can_focus);
}

void WindowTreeClientImpl::SetPredefinedCursor(Id window_id,
                                               mus::mojom::Cursor cursor_id) {
  DCHECK(tree_);

  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  // We make an inflight change thing here.
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new InFlightPredefinedCursorChange(window, window->predefined_cursor())));
  tree_->SetPredefinedCursor(change_id, window_id, cursor_id);
}

void WindowTreeClientImpl::SetVisible(Window* window, bool visible) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightVisibleChange(window, !visible)));
  tree_->SetWindowVisibility(change_id, server_id(window), visible);
}

void WindowTreeClientImpl::SetOpacity(Window* window, float opacity) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightOpacityChange(window, window->opacity())));
  tree_->SetWindowOpacity(change_id, server_id(window), opacity);
}

void WindowTreeClientImpl::SetProperty(Window* window,
                                       const std::string& name,
                                       mojo::Array<uint8_t> data) {
  DCHECK(tree_);

  mojo::Array<uint8_t> old_value(nullptr);
  if (window->HasSharedProperty(name))
    old_value = mojo::Array<uint8_t>::From(window->properties_[name]);

  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightPropertyChange(window, name, old_value)));
  tree_->SetWindowProperty(change_id, server_id(window), mojo::String(name),
                           std::move(data));
}

void WindowTreeClientImpl::SetWindowTextInputState(
    Id window_id,
    mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetWindowTextInputState(window_id, std::move(state));
}

void WindowTreeClientImpl::SetImeVisibility(Id window_id,
                                            bool visible,
                                            mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetImeVisibility(window_id, visible, std::move(state));
}

void WindowTreeClientImpl::Embed(
    Id window_id,
    mojom::WindowTreeClientPtr client,
    const mojom::WindowTree::EmbedCallback& callback) {
  DCHECK(tree_);
  tree_->Embed(window_id, std::move(client), callback);
}

void WindowTreeClientImpl::RequestClose(Window* window) {
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmRequestClose(server_id(window));
}

void WindowTreeClientImpl::AttachSurface(
    Id window_id,
    mojom::SurfaceType type,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {
  DCHECK(tree_);
  tree_->AttachSurface(window_id, type, std::move(surface), std::move(client));
}

void WindowTreeClientImpl::LocalSetCapture(Window* window) {
  if (capture_window_ == window)
    return;
  Window* lost_capture = capture_window_;
  capture_window_ = window;
  if (lost_capture) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(lost_capture).observers(),
                      OnWindowLostCapture(lost_capture));
  }
}

void WindowTreeClientImpl::LocalSetFocus(Window* focused) {
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
  FOR_EACH_OBSERVER(WindowTreeConnectionObserver, observers_,
                    OnWindowTreeFocusChanged(focused, blurred));
}

void WindowTreeClientImpl::AddWindow(Window* window) {
  DCHECK(windows_.find(server_id(window)) == windows_.end());
  windows_[server_id(window)] = window;
}

void WindowTreeClientImpl::OnWindowDestroyed(Window* window) {
  windows_.erase(server_id(window));

  // Remove any InFlightChanges associated with the window.
  std::set<uint32_t> in_flight_change_ids_to_remove;
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window() == window)
      in_flight_change_ids_to_remove.insert(pair.first);
  }
  for (auto change_id : in_flight_change_ids_to_remove)
    in_flight_map_.erase(change_id);

  if (roots_.erase(window) > 0 && roots_.empty() && delete_on_no_roots_ &&
      !in_destructor_) {
    delete this;
  }
}

Window* WindowTreeClientImpl::GetWindowByServerId(Id id) {
  IdToWindowMap::const_iterator it = windows_.find(id);
  return it != windows_.end() ? it->second : NULL;
}

InFlightChange* WindowTreeClientImpl::GetOldestInFlightChangeMatching(
    const InFlightChange& change) {
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window() == change.window() &&
        pair.second->change_type() == change.change_type() &&
        pair.second->Matches(change)) {
      return pair.second.get();
    }
  }
  return nullptr;
}

uint32_t WindowTreeClientImpl::ScheduleInFlightChange(
    std::unique_ptr<InFlightChange> change) {
  DCHECK(!change->window() ||
         windows_.count(change->window()->server_id()) > 0);
  const uint32_t change_id = next_change_id_++;
  in_flight_map_[change_id] = std::move(change);
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

Window* WindowTreeClientImpl::NewWindowImpl(
    NewWindowType type,
    const Window::SharedProperties* properties) {
  DCHECK(tree_);
  Window* window =
      new Window(this, MakeTransportId(connection_id_, next_window_id_++));
  if (properties)
    window->properties_ = *properties;
  AddWindow(window);

  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(window, type == NewWindowType::CHILD
                                          ? ChangeType::NEW_WINDOW
                                          : ChangeType::NEW_TOP_LEVEL_WINDOW)));
  mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties;
  if (properties) {
    transport_properties =
        mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(*properties);
  }
  if (type == NewWindowType::CHILD) {
    tree_->NewWindow(change_id, server_id(window),
                     std::move(transport_properties));
  } else {
    roots_.insert(window);
    tree_->NewTopLevelWindow(change_id, server_id(window),
                             std::move(transport_properties));
  }
  return window;
}

void WindowTreeClientImpl::OnEmbedImpl(mojom::WindowTree* window_tree,
                                       ConnectionSpecificId connection_id,
                                       mojom::WindowDataPtr root_data,
                                       Id focused_window_id,
                                       bool drawn) {
  // WARNING: this is only called if WindowTreeClientImpl was created as the
  // result of an embedding.
  tree_ = window_tree;
  connection_id_ = connection_id;

  DCHECK(roots_.empty());
  Window* root = AddWindowToConnection(this, nullptr, root_data);
  roots_.insert(root);

  focused_window_ = GetWindowByServerId(focused_window_id);

  WindowPrivate(root).LocalSetParentDrawn(drawn);

  delegate_->OnEmbed(root);

  if (focused_window_) {
    FOR_EACH_OBSERVER(WindowTreeConnectionObserver, observers_,
                      OnWindowTreeFocusChanged(focused_window_, nullptr));
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClientImpl, WindowTreeConnection implementation:

void WindowTreeClientImpl::SetDeleteOnNoRoots(bool value) {
  delete_on_no_roots_ = value;
}

const std::set<Window*>& WindowTreeClientImpl::GetRoots() {
  return roots_;
}

Window* WindowTreeClientImpl::GetFocusedWindow() {
  return focused_window_;
}

void WindowTreeClientImpl::ClearFocus() {
  if (!focused_window_)
    return;

  SetFocus(nullptr);
}

void WindowTreeClientImpl::SetEventObserver(mojom::EventMatcherPtr matcher) {
  if (matcher.is_null()) {
    has_event_observer_ = false;
    tree_->SetEventObserver(nullptr, 0u);
  } else {
    has_event_observer_ = true;
    event_observer_id_++;
    tree_->SetEventObserver(std::move(matcher), event_observer_id_);
  }
}

Window* WindowTreeClientImpl::NewWindow(
    const Window::SharedProperties* properties) {
  return NewWindowImpl(NewWindowType::CHILD, properties);
}

Window* WindowTreeClientImpl::NewTopLevelWindow(
    const Window::SharedProperties* properties) {
  Window* window = NewWindowImpl(NewWindowType::TOP_LEVEL, properties);
  // Assume newly created top level windows are drawn by default, otherwise
  // requests to focus will fail. We will get the real value in
  // OnTopLevelCreated().
  window->LocalSetParentDrawn(true);
  return window;
}

ConnectionSpecificId WindowTreeClientImpl::GetConnectionId() {
  return connection_id_;
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClientImpl, WindowTreeClient implementation:

void WindowTreeClientImpl::AddObserver(WindowTreeConnectionObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeClientImpl::RemoveObserver(
    WindowTreeConnectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowTreeClientImpl::OnEmbed(ConnectionSpecificId connection_id,
                                   mojom::WindowDataPtr root_data,
                                   mojom::WindowTreePtr tree,
                                   Id focused_window_id,
                                   bool drawn) {
  DCHECK(!tree_ptr_);
  tree_ptr_ = std::move(tree);
  tree_ptr_.set_connection_error_handler([this]() { delete this; });

  if (window_manager_delegate_) {
    tree_ptr_->GetWindowManagerClient(GetProxy(&window_manager_internal_client_,
                                               tree_ptr_.associated_group()));
  }

  OnEmbedImpl(tree_ptr_.get(), connection_id, std::move(root_data),
              focused_window_id, drawn);
}

void WindowTreeClientImpl::OnEmbeddedAppDisconnected(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window).observers(),
                      OnWindowEmbeddedAppDisconnected(window));
  }
}

void WindowTreeClientImpl::OnUnembed(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  delegate_->OnUnembed(window);
  WindowPrivate(window).LocalDestroy();
}

void WindowTreeClientImpl::OnLostCapture(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightCaptureChange reset_change(this, nullptr);
  if (ApplyServerChangeToExistingInFlightChange(reset_change))
    return;

  LocalSetCapture(nullptr);
}

void WindowTreeClientImpl::OnTopLevelCreated(uint32_t change_id,
                                             mojom::WindowDataPtr data,
                                             bool drawn) {
  // The server ack'd the top level window we created and supplied the state
  // of the window at the time the server created it. For properties we do not
  // have changes in flight for we can update them immediately. For properties
  // with changes in flight we set the revert value from the server.

  if (!in_flight_map_.count(change_id)) {
    // The window may have been destroyed locally before the server could finish
    // creating the window, and before the server received the notification that
    // the window has been destroyed.
    return;
  }
  std::unique_ptr<InFlightChange> change(std::move(in_flight_map_[change_id]));
  in_flight_map_.erase(change_id);

  Window* window = change->window();
  WindowPrivate window_private(window);

  // Drawn state and ViewportMetrics always come from the server (they can't
  // be modified locally).
  window_private.LocalSetParentDrawn(drawn);
  window_private.LocalSetViewportMetrics(mojom::ViewportMetrics(),
                                         *data->viewport_metrics);

  // The default visibilty is false, we only need update visibility if it
  // differs from that.
  if (data->visible) {
    InFlightVisibleChange visible_change(window, data->visible);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(visible_change);
    if (current_change)
      current_change->SetRevertValueFrom(visible_change);
    else
      window_private.LocalSetVisible(true);
  }

  const gfx::Rect bounds(data->bounds.To<gfx::Rect>());
  {
    InFlightBoundsChange bounds_change(window, bounds);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(bounds_change);
    if (current_change)
      current_change->SetRevertValueFrom(bounds_change);
    else if (window->bounds() != bounds)
      window_private.LocalSetBounds(window->bounds(), bounds);
  }

  // There is currently no API to bulk set properties, so we iterate over each
  // property individually.
  Window::SharedProperties properties =
      data->properties.To<std::map<std::string, std::vector<uint8_t>>>();
  for (const auto& pair : properties) {
    InFlightPropertyChange property_change(
        window, pair.first, mojo::Array<uint8_t>::From(pair.second));
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(property_change);
    if (current_change)
      current_change->SetRevertValueFrom(property_change);
    else
      window_private.LocalSetSharedProperty(pair.first, &(pair.second));
  }

  // Top level windows should not have a parent.
  DCHECK_EQ(0u, data->parent_id);
}

void WindowTreeClientImpl::OnWindowBoundsChanged(Id window_id,
                                                 mojo::RectPtr old_bounds,
                                                 mojo::RectPtr new_bounds) {
  Window* window = GetWindowByServerId(window_id);
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
    mojo::InsetsPtr new_client_area,
    mojo::Array<mojo::RectPtr> new_additional_client_areas) {
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    WindowPrivate(window).LocalSetClientArea(
        new_client_area.To<gfx::Insets>(),
        new_additional_client_areas.To<std::vector<gfx::Rect>>());
  }
}

void WindowTreeClientImpl::OnTransientWindowAdded(
    uint32_t window_id,
    uint32_t transient_window_id) {
  Window* window = GetWindowByServerId(window_id);
  Window* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight add from the server.
  if (window && transient_window)
    WindowPrivate(window).LocalAddTransientWindow(transient_window);
}

void WindowTreeClientImpl::OnTransientWindowRemoved(
    uint32_t window_id,
    uint32_t transient_window_id) {
  Window* window = GetWindowByServerId(window_id);
  Window* transient_window = GetWindowByServerId(transient_window_id);
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
    mojo::Array<uint32_t> window_ids,
    mojom::ViewportMetricsPtr old_metrics,
    mojom::ViewportMetricsPtr new_metrics) {
  for (size_t i = 0; i < window_ids.size(); ++i) {
    Window* window = GetWindowByServerId(window_ids[i]);
    if (window)
      SetViewportMetricsOnDecendants(window, *old_metrics, *new_metrics);
  }
}

void WindowTreeClientImpl::OnWindowHierarchyChanged(
    Id window_id,
    Id old_parent_id,
    Id new_parent_id,
    mojo::Array<mojom::WindowDataPtr> windows) {
  Window* initial_parent =
      windows.size() ? GetWindowByServerId(windows[0]->parent_id) : NULL;

  const bool was_window_known = GetWindowByServerId(window_id) != nullptr;

  BuildWindowTree(this, windows, initial_parent);

  // If the window was not known, then BuildWindowTree() will have created it
  // and parented the window.
  if (!was_window_known)
    return;

  Window* new_parent = GetWindowByServerId(new_parent_id);
  Window* old_parent = GetWindowByServerId(old_parent_id);
  Window* window = GetWindowByServerId(window_id);
  if (new_parent)
    WindowPrivate(new_parent).LocalAddChild(window);
  else
    WindowPrivate(old_parent).LocalRemoveChild(window);
}

void WindowTreeClientImpl::OnWindowReordered(Id window_id,
                                             Id relative_window_id,
                                             mojom::OrderDirection direction) {
  Window* window = GetWindowByServerId(window_id);
  Window* relative_window = GetWindowByServerId(relative_window_id);
  if (window && relative_window)
    WindowPrivate(window).LocalReorder(relative_window, direction);
}

void WindowTreeClientImpl::OnWindowDeleted(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (window)
    WindowPrivate(window).LocalDestroy();
}

Window* WindowTreeClientImpl::GetCaptureWindow() {
  return capture_window_;
}

void WindowTreeClientImpl::OnWindowVisibilityChanged(Id window_id,
                                                     bool visible) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightVisibleChange new_change(window, visible);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetVisible(visible);
}

void WindowTreeClientImpl::OnWindowOpacityChanged(Id window_id,
                                                  float old_opacity,
                                                  float new_opacity) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightOpacityChange new_change(window, new_opacity);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetOpacity(new_opacity);
}

void WindowTreeClientImpl::OnWindowParentDrawnStateChanged(Id window_id,
                                                           bool drawn) {
  Window* window = GetWindowByServerId(window_id);
  if (window)
    WindowPrivate(window).LocalSetParentDrawn(drawn);
}

void WindowTreeClientImpl::OnWindowSharedPropertyChanged(
    Id window_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightPropertyChange new_change(window, name, new_data);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetSharedProperty(name, std::move(new_data));
}

void WindowTreeClientImpl::OnWindowInputEvent(uint32_t event_id,
                                              Id window_id,
                                              mojom::EventPtr event,
                                              uint32_t event_observer_id) {
  std::unique_ptr<ui::Event> ui_event = event.To<std::unique_ptr<ui::Event>>();

  // Non-zero event_observer_id means it matched an event observer on the
  // server.
  if (event_observer_id != 0 && has_event_observer_ &&
      event_observer_id == event_observer_id_)
    delegate_->OnEventObserved(*ui_event);

  Window* window = GetWindowByServerId(window_id);
  if (!window || !window->input_event_handler_) {
    tree_->OnWindowInputEventAck(event_id, mojom::EventResult::UNHANDLED);
    return;
  }

  std::unique_ptr<base::Callback<void(mojom::EventResult)>> ack_callback(
      new base::Callback<void(mojom::EventResult)>(
          base::Bind(&mojom::WindowTree::OnWindowInputEventAck,
                     base::Unretained(tree_), event_id)));
  window->input_event_handler_->OnWindowInputEvent(
      window, *event.To<std::unique_ptr<ui::Event>>().get(), &ack_callback);

  // The handler did not take ownership of the callback, so we send the ack,
  // marking the event as not consumed.
  if (ack_callback)
    ack_callback->Run(mojom::EventResult::UNHANDLED);
}

void WindowTreeClientImpl::OnEventObserved(mojom::EventPtr event,
                                           uint32_t event_observer_id) {
  if (has_event_observer_ && event_observer_id == event_observer_id_) {
    std::unique_ptr<ui::Event> ui_event =
        event.To<std::unique_ptr<ui::Event>>();
    delegate_->OnEventObserved(*ui_event);
  }
}

void WindowTreeClientImpl::OnWindowFocused(Id focused_window_id) {
  Window* focused_window = GetWindowByServerId(focused_window_id);
  InFlightFocusChange new_change(this, focused_window);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  LocalSetFocus(focused_window);
}

void WindowTreeClientImpl::OnWindowPredefinedCursorChanged(
    Id window_id,
    mojom::Cursor cursor) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightPredefinedCursorChange new_change(window, cursor);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetPredefinedCursor(cursor);
}

void WindowTreeClientImpl::OnChangeCompleted(uint32_t change_id, bool success) {
  std::unique_ptr<InFlightChange> change(std::move(in_flight_map_[change_id]));
  in_flight_map_.erase(change_id);
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

void WindowTreeClientImpl::GetWindowManager(
    mojo::AssociatedInterfaceRequest<WindowManager> internal) {
  window_manager_internal_.reset(
      new mojo::AssociatedBinding<mojom::WindowManager>(this,
                                                        std::move(internal)));
}

void WindowTreeClientImpl::RequestClose(uint32_t window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window || !IsRoot(window))
    return;

  FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window).observers(),
                    OnRequestClose(window));
}

void WindowTreeClientImpl::WmSetBounds(uint32_t change_id,
                                       Id window_id,
                                       mojo::RectPtr transit_bounds) {
  Window* window = GetWindowByServerId(window_id);
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
  window_manager_internal_client_->WmResponse(change_id, result);
}

void WindowTreeClientImpl::WmSetProperty(uint32_t change_id,
                                         Id window_id,
                                         const mojo::String& name,
                                         mojo::Array<uint8_t> transit_data) {
  Window* window = GetWindowByServerId(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    std::unique_ptr<std::vector<uint8_t>> data;
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
  window_manager_internal_client_->WmResponse(change_id, result);
}

void WindowTreeClientImpl::WmCreateTopLevelWindow(
    uint32_t change_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  std::map<std::string, std::vector<uint8_t>> properties =
      transport_properties.To<std::map<std::string, std::vector<uint8_t>>>();
  Window* window =
      window_manager_delegate_->OnWmCreateTopLevelWindow(&properties);
  window_manager_internal_client_->OnWmCreatedTopLevelWindow(change_id,
                                                             server_id(window));
}

void WindowTreeClientImpl::OnAccelerator(uint32_t id, mojom::EventPtr event) {
  window_manager_delegate_->OnAccelerator(
      id, *event.To<std::unique_ptr<ui::Event>>().get());
}

void WindowTreeClientImpl::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  window_manager_internal_client_->WmSetFrameDecorationValues(
      std::move(values));
}

void WindowTreeClientImpl::AddAccelerator(
    uint32_t id,
    mojom::EventMatcherPtr event_matcher,
    const base::Callback<void(bool)>& callback) {
  window_manager_internal_client_->AddAccelerator(id, std::move(event_matcher),
                                                  callback);
}

void WindowTreeClientImpl::RemoveAccelerator(uint32_t id) {
  window_manager_internal_client_->RemoveAccelerator(id);
}

void WindowTreeClientImpl::AddActivationParent(Window* window) {
  window_manager_internal_client_->AddActivationParent(server_id(window));
}

void WindowTreeClientImpl::RemoveActivationParent(Window* window) {
  window_manager_internal_client_->RemoveActivationParent(server_id(window));
}

void WindowTreeClientImpl::ActivateNextWindow() {
  window_manager_internal_client_->ActivateNextWindow();
}

void WindowTreeClientImpl::SetUnderlaySurfaceOffsetAndExtendedHitArea(
    Window* window,
    const gfx::Vector2d& offset,
    const gfx::Insets& hit_area) {
  window_manager_internal_client_->SetUnderlaySurfaceOffsetAndExtendedHitArea(
      server_id(window), offset.x(), offset.y(), mojo::Insets::From(hit_area));
}

}  // namespace mus
