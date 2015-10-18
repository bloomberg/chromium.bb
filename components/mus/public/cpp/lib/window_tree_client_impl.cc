// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"

#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"

namespace mus {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local window object from transport data.
Window* AddWindowToConnection(WindowTreeClientImpl* client,
                              Window* parent,
                              const mojom::WindowDataPtr& window_data) {
  // We don't use the cto that takes a WindowTreeConnection here, since it will
  // call back to the service and attempt to create a new view.
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
  private_window.LocalSetBounds(mojo::Rect(), *window_data->bounds);
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
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request,
    CreateType create_type) {
  WindowTreeClientImpl* client =
      new WindowTreeClientImpl(delegate, request.Pass());
  if (create_type == CreateType::WAIT_FOR_EMBED)
    client->WaitForEmbed();
  return client;
}

WindowTreeClientImpl::WindowTreeClientImpl(
    WindowTreeDelegate* delegate,
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request)
    : connection_id_(0),
      next_id_(1),
      delegate_(delegate),
      root_(nullptr),
      capture_window_(nullptr),
      focused_window_(nullptr),
      activated_window_(nullptr),
      binding_(this, request.Pass()),
      is_embed_root_(false),
      in_destructor_(false) {}

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

  // Delete the non-owned views last. In the typical case these are roots. The
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

void WindowTreeClientImpl::SetBounds(Id window_id, const mojo::Rect& bounds) {
  DCHECK(tree_);
  tree_->SetWindowBounds(window_id, bounds.Clone(), ActionCompletedCallback());
}

void WindowTreeClientImpl::SetClientArea(Id window_id,
                                         const mojo::Rect& client_area) {
  DCHECK(tree_);
  tree_->SetClientArea(window_id, client_area.Clone());
}

void WindowTreeClientImpl::SetFocus(Id window_id) {
  // In order for us to get here we had to have exposed a window, which implies
  // we
  // got a connection.
  DCHECK(tree_);
  tree_->SetFocus(window_id);
}

void WindowTreeClientImpl::SetVisible(Id window_id, bool visible) {
  DCHECK(tree_);
  tree_->SetWindowVisibility(window_id, visible, ActionCompletedCallback());
}

void WindowTreeClientImpl::SetProperty(Id window_id,
                                       const std::string& name,
                                       const std::vector<uint8_t>& data) {
  DCHECK(tree_);
  tree_->SetWindowProperty(window_id, mojo::String(name),
                           mojo::Array<uint8_t>::From(data),
                           ActionCompletedCallback());
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
    mus::mojom::WindowTreeClientPtr client,
    uint32_t policy_bitmask,
    const mus::mojom::WindowTree::EmbedCallback& callback) {
  DCHECK(tree_);
  tree_->Embed(window_id, client.Pass(), policy_bitmask, callback);
}

void WindowTreeClientImpl::RequestSurface(
    Id window_id,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {
  DCHECK(tree_);
  tree_->RequestSurface(window_id, surface.Pass(), client.Pass());
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
}

void WindowTreeClientImpl::OnRootDestroyed(Window* root) {
  DCHECK_EQ(root, root_);
  root_ = nullptr;

  // When the root is gone we can't do anything useful.
  if (!in_destructor_)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClientImpl, WindowTreeConnection implementation:

Id WindowTreeClientImpl::CreateWindowOnServer() {
  DCHECK(tree_);
  const Id window_id = MakeTransportId(connection_id_, next_id_++);
  tree_->NewWindow(window_id, [this](mojom::ErrorCode code) {
    OnActionCompleted(code == mojom::ERROR_CODE_NONE);
  });
  return window_id;
}

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
  Window* window = new Window(this, CreateWindowOnServer());
  AddWindow(window);
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
                                   mus::mojom::WindowTreePtr tree,
                                   Id focused_window_id,
                                   uint32 access_policy) {
  if (tree) {
    DCHECK(!tree_);
    tree_ = tree.Pass();
    tree_.set_connection_error_handler([this]() { delete this; });
  }
  connection_id_ = connection_id;
  is_embed_root_ =
      (access_policy & mus::mojom::WindowTree::ACCESS_POLICY_EMBED_ROOT) != 0;

  DCHECK(!root_);
  root_ = AddWindowToConnection(this, nullptr, root_data);

  focused_window_ = GetWindowById(focused_window_id);

  delegate_->OnEmbed(root_);
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
  WindowPrivate(window).LocalSetBounds(*old_bounds, *new_bounds);
}

void WindowTreeClientImpl::OnClientAreaChanged(uint32_t window_id,
                                               mojo::RectPtr old_client_area,
                                               mojo::RectPtr new_client_area) {
  Window* window = GetWindowById(window_id);
  if (window)
    WindowPrivate(window).LocalSetClientArea(*new_client_area);
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
  // and
  // parented the window.
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
  if (window) {
    std::vector<uint8_t> data;
    std::vector<uint8_t>* data_ptr = NULL;
    if (!new_data.is_null()) {
      data = new_data.To<std::vector<uint8_t>>();
      data_ptr = &data;
    }

    window->SetSharedProperty(name, data_ptr);
  }
}

void WindowTreeClientImpl::OnWindowInputEvent(
    Id window_id,
    mojo::EventPtr event,
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
