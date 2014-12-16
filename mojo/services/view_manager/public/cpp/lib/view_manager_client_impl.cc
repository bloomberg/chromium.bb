// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/public/cpp/lib/view_manager_client_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/view_manager/public/cpp/lib/view_private.h"
#include "mojo/services/view_manager/public/cpp/util.h"
#include "mojo/services/view_manager/public/cpp/view_manager_delegate.h"
#include "mojo/services/view_manager/public/cpp/view_observer.h"

namespace mojo {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local view object from transport data.
View* AddViewToViewManager(ViewManagerClientImpl* client,
                           View* parent,
                           const ViewDataPtr& view_data) {
  // We don't use the ctor that takes a ViewManager here, since it will call
  // back to the service and attempt to create a new view.
  View* view = ViewPrivate::LocalCreate();
  ViewPrivate private_view(view);
  private_view.set_view_manager(client);
  private_view.set_id(view_data->view_id);
  private_view.set_visible(view_data->visible);
  private_view.set_drawn(view_data->drawn);
  private_view.set_properties(
      view_data->properties.To<std::map<std::string, std::vector<uint8_t>>>());
  client->AddView(view);
  private_view.LocalSetBounds(Rect(), *view_data->bounds);
  if (parent)
    ViewPrivate(parent).LocalAddChild(view);
  return view;
}

View* BuildViewTree(ViewManagerClientImpl* client,
                    const Array<ViewDataPtr>& views,
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
    View* view = AddViewToViewManager(
        client, !parents.empty() ? parents.back() : NULL, views[i]);
    if (!last_view)
      root = view;
    last_view = view;
  }
  return root;
}

// Responsible for removing a root from the ViewManager when that view is
// destroyed.
class RootObserver : public ViewObserver {
 public:
  explicit RootObserver(View* root) : root_(root) {}
  ~RootObserver() override {}

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    DCHECK_EQ(view, root_);
    static_cast<ViewManagerClientImpl*>(
        ViewPrivate(root_).view_manager())->RootDestroyed(root_);
    view->RemoveObserver(this);
    delete this;
  }

  View* root_;

  DISALLOW_COPY_AND_ASSIGN(RootObserver);
};

ViewManagerClientImpl::ViewManagerClientImpl(ViewManagerDelegate* delegate,
                                             Shell* shell,
                                             ScopedMessagePipeHandle handle,
                                             bool delete_on_error)
    : connected_(false),
      connection_id_(0),
      next_id_(1),
      delegate_(delegate),
      root_(nullptr),
      focused_view_(nullptr),
      binding_(this, handle.Pass()),
      service_(binding_.client()),
      delete_on_error_(delete_on_error) {
}

ViewManagerClientImpl::~ViewManagerClientImpl() {
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
  // exception is the window manager, which may know aboutother random views
  // that it doesn't own.
  // NOTE: we manually delete as we're a friend.
  for (size_t i = 0; i < non_owned.size(); ++i)
    delete non_owned[i];

  delegate_->OnViewManagerDisconnected(this);
}

Id ViewManagerClientImpl::CreateView() {
  DCHECK(connected_);
  const Id view_id = MakeTransportId(connection_id_, ++next_id_);
  service_->CreateView(view_id, ActionCompletedCallbackWithErrorCode());
  return view_id;
}

void ViewManagerClientImpl::DestroyView(Id view_id) {
  DCHECK(connected_);
  service_->DeleteView(view_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::AddChild(Id child_id, Id parent_id) {
  DCHECK(connected_);
  service_->AddView(parent_id, child_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::RemoveChild(Id child_id, Id parent_id) {
  DCHECK(connected_);
  service_->RemoveViewFromParent(child_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::Reorder(
    Id view_id,
    Id relative_view_id,
    OrderDirection direction) {
  DCHECK(connected_);
  service_->ReorderView(view_id, relative_view_id, direction,
                        ActionCompletedCallback());
}

bool ViewManagerClientImpl::OwnsView(Id id) const {
  return HiWord(id) == connection_id_;
}

void ViewManagerClientImpl::SetBounds(Id view_id, const Rect& bounds) {
  DCHECK(connected_);
  service_->SetViewBounds(view_id, bounds.Clone(), ActionCompletedCallback());
}

void ViewManagerClientImpl::SetSurfaceId(Id view_id, SurfaceIdPtr surface_id) {
  DCHECK(connected_);
  if (surface_id.is_null())
    return;
  service_->SetViewSurfaceId(
      view_id, surface_id.Pass(), ActionCompletedCallback());
}

void ViewManagerClientImpl::SetFocus(Id view_id) {
  // In order for us to get here we had to have exposed a view, which implies we
  // got a connection.
  DCHECK(window_manager_.get());
  window_manager_->FocusWindow(view_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::SetVisible(Id view_id, bool visible) {
  DCHECK(connected_);
  service_->SetViewVisibility(view_id, visible, ActionCompletedCallback());
}

void ViewManagerClientImpl::SetProperty(
    Id view_id,
    const std::string& name,
    const std::vector<uint8_t>& data) {
  DCHECK(connected_);
  service_->SetViewProperty(view_id,
                            String(name),
                            Array<uint8_t>::From(data),
                            ActionCompletedCallback());
}

void ViewManagerClientImpl::Embed(const String& url, Id view_id) {
  ServiceProviderPtr sp;
  BindToProxy(new ServiceProviderImpl, &sp);
  Embed(url, view_id, sp.Pass());
}

void ViewManagerClientImpl::Embed(
    const String& url,
    Id view_id,
    ServiceProviderPtr service_provider) {
  DCHECK(connected_);
  service_->Embed(url, view_id,
      MakeRequest<ServiceProvider>(service_provider.PassMessagePipe()),
      ActionCompletedCallback());
}

void ViewManagerClientImpl::AddView(View* view) {
  DCHECK(views_.find(view->id()) == views_.end());
  views_[view->id()] = view;
}

void ViewManagerClientImpl::RemoveView(Id view_id) {
  IdToViewMap::iterator it = views_.find(view_id);
  if (it != views_.end())
    views_.erase(it);
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, ViewManager implementation:

const std::string& ViewManagerClientImpl::GetEmbedderURL() const {
  return creator_url_;
}

View* ViewManagerClientImpl::GetRoot() {
  return root_;
}

View* ViewManagerClientImpl::GetViewById(Id id) {
  IdToViewMap::const_iterator it = views_.find(id);
  return it != views_.end() ? it->second : NULL;
}

View* ViewManagerClientImpl::GetFocusedView() {
  return focused_view_;
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, ViewManagerClient implementation:

void ViewManagerClientImpl::OnEmbed(
    ConnectionSpecificId connection_id,
    const String& creator_url,
    ViewDataPtr root_data,
    InterfaceRequest<ServiceProvider> parent_services,
    ScopedMessagePipeHandle window_manager_pipe) {
  DCHECK(!connected_);
  connected_ = true;
  connection_id_ = connection_id;
  creator_url_ = String::From(creator_url);

  DCHECK(!root_);
  root_ = AddViewToViewManager(this, nullptr, root_data);
  root_->AddObserver(new RootObserver(root_));

  ServiceProviderImpl* exported_services = nullptr;
  scoped_ptr<ServiceProvider> remote;

  if (parent_services.is_pending()) {
    // BindToRequest() binds the lifetime of |exported_services| to the pipe.
    exported_services = new ServiceProviderImpl;
    BindToRequest(exported_services, &parent_services);
    remote.reset(exported_services->CreateRemoteServiceProvider());
  }
  window_manager_.Bind(window_manager_pipe.Pass());
  window_manager_.set_client(this);
  // base::Unretained() is safe here as |window_manager_| is bound to our
  // lifetime.
  window_manager_->GetFocusedAndActiveViews(
      base::Bind(&ViewManagerClientImpl::OnGotFocusedAndActiveViews,
                 base::Unretained(this)));
  delegate_->OnEmbed(this, root_, exported_services, remote.Pass());
}

void ViewManagerClientImpl::OnEmbeddedAppDisconnected(Id view_id) {
  View* view = GetViewById(view_id);
  if (view) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view).observers(),
                      OnViewEmbeddedAppDisconnected(view));
  }
}

void ViewManagerClientImpl::OnViewBoundsChanged(Id view_id,
                                                RectPtr old_bounds,
                                                RectPtr new_bounds) {
  View* view = GetViewById(view_id);
  ViewPrivate(view).LocalSetBounds(*old_bounds, *new_bounds);
}

void ViewManagerClientImpl::OnViewHierarchyChanged(
    Id view_id,
    Id new_parent_id,
    Id old_parent_id,
    mojo::Array<ViewDataPtr> views) {
  View* initial_parent = views.size() ?
      GetViewById(views[0]->parent_id) : NULL;

  BuildViewTree(this, views, initial_parent);

  View* new_parent = GetViewById(new_parent_id);
  View* old_parent = GetViewById(old_parent_id);
  View* view = GetViewById(view_id);
  if (new_parent)
    ViewPrivate(new_parent).LocalAddChild(view);
  else
    ViewPrivate(old_parent).LocalRemoveChild(view);
}

void ViewManagerClientImpl::OnViewReordered(Id view_id,
                                            Id relative_view_id,
                                            OrderDirection direction) {
  View* view = GetViewById(view_id);
  View* relative_view = GetViewById(relative_view_id);
  if (view && relative_view)
    ViewPrivate(view).LocalReorder(relative_view, direction);
}

void ViewManagerClientImpl::OnViewDeleted(Id view_id) {
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalDestroy();
}

void ViewManagerClientImpl::OnViewVisibilityChanged(Id view_id, bool visible) {
  // TODO(sky): there is a race condition here. If this client and another
  // client change the visibility at the same time the wrong value may be set.
  // Deal with this some how.
  View* view = GetViewById(view_id);
  if (view)
    view->SetVisible(visible);
}

void ViewManagerClientImpl::OnViewDrawnStateChanged(Id view_id, bool drawn) {
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalSetDrawn(drawn);
}

void ViewManagerClientImpl::OnViewSharedPropertyChanged(
    Id view_id,
    const String& name,
    Array<uint8_t> new_data) {
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

void ViewManagerClientImpl::OnViewInputEvent(
    Id view_id,
    EventPtr event,
    const Callback<void()>& ack_callback) {
  View* view = GetViewById(view_id);
  if (view) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view).observers(),
                      OnViewInputEvent(view, event));
  }
  ack_callback.Run();
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, WindowManagerClient implementation:

void ViewManagerClientImpl::OnCaptureChanged(Id old_capture_view_id,
                                             Id new_capture_view_id) {}

void ViewManagerClientImpl::OnFocusChanged(Id old_focused_view_id,
                                           Id new_focused_view_id) {
  View* focused = GetViewById(new_focused_view_id);
  View* blurred = GetViewById(old_focused_view_id);
  if (blurred) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(blurred).observers(),
                      OnViewFocusChanged(focused, blurred));
  }
  focused_view_ = focused;
  if (focused) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(focused).observers(),
                      OnViewFocusChanged(focused, blurred));
  }
}

void ViewManagerClientImpl::OnActiveWindowChanged(Id old_focused_window,
                                                  Id new_focused_window) {}

////////////////////////////////////////////////////////////////////////////////
// OnConnectionError, private:
void ViewManagerClientImpl::OnConnectionError() {
  if (delete_on_error_)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, private:

void ViewManagerClientImpl::RootDestroyed(View* root) {
  DCHECK_EQ(root, root_);
  root_ = nullptr;
}

void ViewManagerClientImpl::OnActionCompleted(bool success) {
  if (!change_acked_callback_.is_null())
    change_acked_callback_.Run();
}

void ViewManagerClientImpl::OnActionCompletedWithErrorCode(ErrorCode code) {
  OnActionCompleted(code == ERROR_CODE_NONE);
}

base::Callback<void(bool)> ViewManagerClientImpl::ActionCompletedCallback() {
  return base::Bind(&ViewManagerClientImpl::OnActionCompleted,
                    base::Unretained(this));
}

base::Callback<void(ErrorCode)>
    ViewManagerClientImpl::ActionCompletedCallbackWithErrorCode() {
  return base::Bind(&ViewManagerClientImpl::OnActionCompletedWithErrorCode,
                    base::Unretained(this));
}

void ViewManagerClientImpl::OnGotFocusedAndActiveViews(uint32 focused_view_id,
                                                       uint32 active_view_id) {
  if (GetViewById(focused_view_id) != focused_view_)
    OnFocusChanged(focused_view_ ? focused_view_->id() : 0, focused_view_id);
}

}  // namespace mojo
