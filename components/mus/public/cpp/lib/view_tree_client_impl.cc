// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/view_tree_client_impl.h"

#include "components/mus/public/cpp/lib/view_private.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"

namespace mus {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local view object from transport data.
View* AddViewToConnection(ViewTreeClientImpl* client,
                          View* parent,
                          const mojo::ViewDataPtr& view_data) {
  // We don't use the cto that takes a ViewTreeConnection here, since it will
  // call back to the service and attempt to create a new view.
  View* view = ViewPrivate::LocalCreate();
  ViewPrivate private_view(view);
  private_view.set_connection(client);
  private_view.set_id(view_data->view_id);
  private_view.set_visible(view_data->visible);
  private_view.set_drawn(view_data->drawn);
  private_view.LocalSetViewportMetrics(mojo::ViewportMetrics(),
                                       *view_data->viewport_metrics);
  private_view.set_properties(
      view_data->properties.To<std::map<std::string, std::vector<uint8_t>>>());
  client->AddView(view);
  private_view.LocalSetBounds(mojo::Rect(), *view_data->bounds);
  if (parent)
    ViewPrivate(parent).LocalAddChild(view);
  return view;
}

View* BuildViewTree(ViewTreeClientImpl* client,
                    const mojo::Array<mojo::ViewDataPtr>& views,
                    View* initial_parent) {
  std::vector<View*> parents;
  View* root = NULL;
  View* last_view = NULL;
  if (initial_parent)
    parents.push_back(initial_parent);
  for (size_t i = 0; i < views.size(); ++i) {
    if (last_view && views[i]->parent_id == last_view->id()) {
      parents.push_back(last_view);
    } else if (!parents.empty()) {
      while (parents.back()->id() != views[i]->parent_id)
        parents.pop_back();
    }
    View* view = AddViewToConnection(
        client, !parents.empty() ? parents.back() : NULL, views[i]);
    if (!last_view)
      root = view;
    last_view = view;
  }
  return root;
}

ViewTreeConnection* ViewTreeConnection::Create(
    ViewTreeDelegate* delegate,
    mojo::InterfaceRequest<mojo::ViewTreeClient> request) {
  return new ViewTreeClientImpl(delegate, request.Pass());
}

ViewTreeClientImpl::ViewTreeClientImpl(
    ViewTreeDelegate* delegate,
    mojo::InterfaceRequest<mojo::ViewTreeClient> request)
    : connection_id_(0),
      next_id_(1),
      delegate_(delegate),
      root_(nullptr),
      capture_view_(nullptr),
      focused_view_(nullptr),
      activated_view_(nullptr),
      binding_(this, request.Pass()),
      is_embed_root_(false),
      in_destructor_(false) {}

ViewTreeClientImpl::~ViewTreeClientImpl() {
  in_destructor_ = true;

  std::vector<View*> non_owned;
  while (!views_.empty()) {
    IdToViewMap::iterator it = views_.begin();
    if (OwnsView(it->second->id())) {
      it->second->Destroy();
    } else {
      non_owned.push_back(it->second);
      views_.erase(it);
    }
  }

  // Delete the non-owned views last. In the typical case these are roots. The
  // exception is the window manager and embed roots, which may know about
  // other random views that it doesn't own.
  // NOTE: we manually delete as we're a friend.
  for (size_t i = 0; i < non_owned.size(); ++i)
    delete non_owned[i];

  delegate_->OnConnectionLost(this);
}

void ViewTreeClientImpl::DestroyView(Id view_id) {
  DCHECK(tree_);
  tree_->DeleteView(view_id, ActionCompletedCallback());
}

void ViewTreeClientImpl::AddChild(Id child_id, Id parent_id) {
  DCHECK(tree_);
  tree_->AddView(parent_id, child_id, ActionCompletedCallback());
}

void ViewTreeClientImpl::RemoveChild(Id child_id, Id parent_id) {
  DCHECK(tree_);
  tree_->RemoveViewFromParent(child_id, ActionCompletedCallback());
}

void ViewTreeClientImpl::Reorder(Id view_id,
                                 Id relative_view_id,
                                 mojo::OrderDirection direction) {
  DCHECK(tree_);
  tree_->ReorderView(view_id, relative_view_id, direction,
                     ActionCompletedCallback());
}

bool ViewTreeClientImpl::OwnsView(Id id) const {
  return HiWord(id) == connection_id_;
}

void ViewTreeClientImpl::SetBounds(Id view_id, const mojo::Rect& bounds) {
  DCHECK(tree_);
  tree_->SetViewBounds(view_id, bounds.Clone(), ActionCompletedCallback());
}

void ViewTreeClientImpl::SetFocus(Id view_id) {
  // In order for us to get here we had to have exposed a view, which implies we
  // got a connection.
  DCHECK(tree_);
  tree_->SetFocus(view_id);
}

void ViewTreeClientImpl::SetVisible(Id view_id, bool visible) {
  DCHECK(tree_);
  tree_->SetViewVisibility(view_id, visible, ActionCompletedCallback());
}

void ViewTreeClientImpl::SetProperty(Id view_id,
                                     const std::string& name,
                                     const std::vector<uint8_t>& data) {
  DCHECK(tree_);
  tree_->SetViewProperty(view_id, mojo::String(name),
                         mojo::Array<uint8_t>::From(data),
                         ActionCompletedCallback());
}

void ViewTreeClientImpl::SetViewTextInputState(Id view_id,
                                               mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetViewTextInputState(view_id, state.Pass());
}

void ViewTreeClientImpl::SetImeVisibility(Id view_id,
                                          bool visible,
                                          mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetImeVisibility(view_id, visible, state.Pass());
}

void ViewTreeClientImpl::Embed(Id view_id,
                               mojo::ViewTreeClientPtr client,
                               uint32_t policy_bitmask,
                               const mojo::ViewTree::EmbedCallback& callback) {
  DCHECK(tree_);
  tree_->Embed(view_id, client.Pass(), policy_bitmask, callback);
}

void ViewTreeClientImpl::RequestSurface(
    Id view_id,
    mojo::InterfaceRequest<mojo::Surface> surface,
    mojo::SurfaceClientPtr client) {
  DCHECK(tree_);
  tree_->RequestSurface(view_id, surface.Pass(), client.Pass());
}

void ViewTreeClientImpl::AddView(View* view) {
  DCHECK(views_.find(view->id()) == views_.end());
  views_[view->id()] = view;
}

void ViewTreeClientImpl::RemoveView(Id view_id) {
  if (focused_view_ && focused_view_->id() == view_id)
    OnViewFocused(0);

  IdToViewMap::iterator it = views_.find(view_id);
  if (it != views_.end())
    views_.erase(it);
}

void ViewTreeClientImpl::OnRootDestroyed(View* root) {
  DCHECK_EQ(root, root_);
  root_ = nullptr;

  // When the root is gone we can't do anything useful.
  if (!in_destructor_)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// ViewTreeClientImpl, ViewTreeConnection implementation:

Id ViewTreeClientImpl::CreateViewOnServer() {
  DCHECK(tree_);
  const Id view_id = MakeTransportId(connection_id_, ++next_id_);
  tree_->CreateView(view_id, [this](mojo::ErrorCode code) {
    OnActionCompleted(code == mojo::ERROR_CODE_NONE);
  });
  return view_id;
}

View* ViewTreeClientImpl::GetRoot() {
  return root_;
}

View* ViewTreeClientImpl::GetViewById(Id id) {
  IdToViewMap::const_iterator it = views_.find(id);
  return it != views_.end() ? it->second : NULL;
}

View* ViewTreeClientImpl::GetFocusedView() {
  return focused_view_;
}

View* ViewTreeClientImpl::CreateView() {
  View* view = new View(this, CreateViewOnServer());
  AddView(view);
  return view;
}

bool ViewTreeClientImpl::IsEmbedRoot() {
  return is_embed_root_;
}

ConnectionSpecificId ViewTreeClientImpl::GetConnectionId() {
  return connection_id_;
}

////////////////////////////////////////////////////////////////////////////////
// ViewTreeClientImpl, ViewTreeClient implementation:

void ViewTreeClientImpl::OnEmbed(ConnectionSpecificId connection_id,
                                 mojo::ViewDataPtr root_data,
                                 mojo::ViewTreePtr tree,
                                 Id focused_view_id,
                                 uint32 access_policy) {
  if (tree) {
    DCHECK(!tree_);
    tree_ = tree.Pass();
    tree_.set_connection_error_handler([this]() { delete this; });
  }
  connection_id_ = connection_id;
  is_embed_root_ =
      (access_policy & mojo::ViewTree::ACCESS_POLICY_EMBED_ROOT) != 0;

  DCHECK(!root_);
  root_ = AddViewToConnection(this, nullptr, root_data);

  focused_view_ = GetViewById(focused_view_id);

  delegate_->OnEmbed(root_);
}

void ViewTreeClientImpl::OnEmbeddedAppDisconnected(Id view_id) {
  View* view = GetViewById(view_id);
  if (view) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view).observers(),
                      OnViewEmbeddedAppDisconnected(view));
  }
}

void ViewTreeClientImpl::OnUnembed() {
  delegate_->OnUnembed();
  // This will send out the various notifications.
  delete this;
}

void ViewTreeClientImpl::OnViewBoundsChanged(Id view_id,
                                             mojo::RectPtr old_bounds,
                                             mojo::RectPtr new_bounds) {
  View* view = GetViewById(view_id);
  ViewPrivate(view).LocalSetBounds(*old_bounds, *new_bounds);
}

namespace {

void SetViewportMetricsOnDecendants(View* root,
                                    const mojo::ViewportMetrics& old_metrics,
                                    const mojo::ViewportMetrics& new_metrics) {
  ViewPrivate(root).LocalSetViewportMetrics(old_metrics, new_metrics);
  const View::Children& children = root->children();
  for (size_t i = 0; i < children.size(); ++i)
    SetViewportMetricsOnDecendants(children[i], old_metrics, new_metrics);
}
}

void ViewTreeClientImpl::OnViewViewportMetricsChanged(
    mojo::ViewportMetricsPtr old_metrics,
    mojo::ViewportMetricsPtr new_metrics) {
  View* view = GetRoot();
  if (view)
    SetViewportMetricsOnDecendants(view, *old_metrics, *new_metrics);
}

void ViewTreeClientImpl::OnViewHierarchyChanged(
    Id view_id,
    Id new_parent_id,
    Id old_parent_id,
    mojo::Array<mojo::ViewDataPtr> views) {
  View* initial_parent = views.size() ? GetViewById(views[0]->parent_id) : NULL;

  const bool was_view_known = GetViewById(view_id) != nullptr;

  BuildViewTree(this, views, initial_parent);

  // If the view was not known, then BuildViewTree() will have created it and
  // parented the view.
  if (!was_view_known)
    return;

  View* new_parent = GetViewById(new_parent_id);
  View* old_parent = GetViewById(old_parent_id);
  View* view = GetViewById(view_id);
  if (new_parent)
    ViewPrivate(new_parent).LocalAddChild(view);
  else
    ViewPrivate(old_parent).LocalRemoveChild(view);
}

void ViewTreeClientImpl::OnViewReordered(Id view_id,
                                         Id relative_view_id,
                                         mojo::OrderDirection direction) {
  View* view = GetViewById(view_id);
  View* relative_view = GetViewById(relative_view_id);
  if (view && relative_view)
    ViewPrivate(view).LocalReorder(relative_view, direction);
}

void ViewTreeClientImpl::OnViewDeleted(Id view_id) {
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalDestroy();
}

void ViewTreeClientImpl::OnViewVisibilityChanged(Id view_id, bool visible) {
  // TODO(sky): there is a race condition here. If this client and another
  // client change the visibility at the same time the wrong value may be set.
  // Deal with this some how.
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalSetVisible(visible);
}

void ViewTreeClientImpl::OnViewDrawnStateChanged(Id view_id, bool drawn) {
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalSetDrawn(drawn);
}

void ViewTreeClientImpl::OnViewSharedPropertyChanged(
    Id view_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  View* view = GetViewById(view_id);
  if (view) {
    std::vector<uint8_t> data;
    std::vector<uint8_t>* data_ptr = NULL;
    if (!new_data.is_null()) {
      data = new_data.To<std::vector<uint8_t>>();
      data_ptr = &data;
    }

    view->SetSharedProperty(name, data_ptr);
  }
}

void ViewTreeClientImpl::OnViewInputEvent(
    Id view_id,
    mojo::EventPtr event,
    const mojo::Callback<void()>& ack_callback) {
  View* view = GetViewById(view_id);
  if (view) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view).observers(),
                      OnViewInputEvent(view, event));
  }
  ack_callback.Run();
}

void ViewTreeClientImpl::OnViewFocused(Id focused_view_id) {
  View* focused = GetViewById(focused_view_id);
  View* blurred = focused_view_;
  // Update |focused_view_| before calling any of the observers, so that the
  // observers get the correct result from calling |View::HasFocus()|,
  // |ViewTreeConnection::GetFocusedView()| etc.
  focused_view_ = focused;
  if (blurred) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(blurred).observers(),
                      OnViewFocusChanged(focused, blurred));
  }
  if (focused) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(focused).observers(),
                      OnViewFocusChanged(focused, blurred));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ViewTreeClientImpl, private:

void ViewTreeClientImpl::OnActionCompleted(bool success) {
  if (!change_acked_callback_.is_null())
    change_acked_callback_.Run();
}

mojo::Callback<void(bool)> ViewTreeClientImpl::ActionCompletedCallback() {
  return [this](bool success) { OnActionCompleted(success); };
}

}  // namespace mus
