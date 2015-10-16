// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/vm/view_tree_host_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/vm/connection_manager.h"
#include "components/mus/vm/display_manager.h"
#include "components/mus/vm/focus_controller.h"
#include "components/mus/vm/view_tree_host_delegate.h"
#include "components/mus/vm/view_tree_impl.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace mus {

ViewTreeHostImpl::ViewTreeHostImpl(
    mojo::ViewTreeHostClientPtr client,
    ConnectionManager* connection_manager,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : delegate_(nullptr),
      connection_manager_(connection_manager),
      client_(client.Pass()),
      event_dispatcher_(this),
      display_manager_(DisplayManager::Create(app_impl,
                                              gpu_state,
                                              surfaces_state)),
      focus_controller_(new FocusController(this)) {
  display_manager_->Init(this);
  if (client_) {
    client_.set_connection_error_handler(
        base::Bind(&ViewTreeHostImpl::OnClientClosed, base::Unretained(this)));
  }
}

ViewTreeHostImpl::~ViewTreeHostImpl() {}

void ViewTreeHostImpl::Init(ViewTreeHostDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_ && root_)
    delegate_->OnDisplayInitialized();
}

ViewTreeImpl* ViewTreeHostImpl::GetViewTree() {
  return delegate_ ? delegate_->GetViewTree() : nullptr;
}

bool ViewTreeHostImpl::IsViewAttachedToRoot(const ServerView* view) const {
  return root_->Contains(view) && view != root_.get();
}

bool ViewTreeHostImpl::SchedulePaintIfInViewport(const ServerView* view,
                                                 const gfx::Rect& bounds) {
  if (root_->Contains(view)) {
    display_manager_->SchedulePaint(view, bounds);
    return true;
  }
  return false;
}

const mojo::ViewportMetrics& ViewTreeHostImpl::GetViewportMetrics() const {
  return display_manager_->GetViewportMetrics();
}

void ViewTreeHostImpl::SetFocusedView(ServerView* new_focused_view) {
  ServerView* old_focused_view = focus_controller_->GetFocusedView();
  if (old_focused_view == new_focused_view)
    return;
  DCHECK(root_view()->Contains(new_focused_view));
  focus_controller_->SetFocusedView(new_focused_view);
  // TODO(beng): have the FocusController notify us via FocusControllerDelegate.
  OnFocusChanged(old_focused_view, new_focused_view);
}

ServerView* ViewTreeHostImpl::GetFocusedView() {
  return focus_controller_->GetFocusedView();
}

void ViewTreeHostImpl::DestroyFocusController() {
  focus_controller_.reset();
}

void ViewTreeHostImpl::UpdateTextInputState(ServerView* view,
                                            const ui::TextInputState& state) {
  // Do not need to update text input for unfocused views.
  if (!display_manager_ || focus_controller_->GetFocusedView() != view)
    return;
  display_manager_->UpdateTextInputState(state);
}

void ViewTreeHostImpl::SetImeVisibility(ServerView* view, bool visible) {
  // Do not need to show or hide IME for unfocused view.
  if (focus_controller_->GetFocusedView() != view)
    return;
  display_manager_->SetImeVisibility(visible);
}

void ViewTreeHostImpl::OnAccelerator(uint32_t accelerator_id,
                                     mojo::EventPtr event) {
  client()->OnAccelerator(accelerator_id, event.Pass());
}

void ViewTreeHostImpl::DispatchInputEventToView(ServerView* target,
                                                mojo::EventPtr event) {
  // If the view is an embed root, forward to the embedded view, not the owner.
  ViewTreeImpl* connection =
      connection_manager_->GetConnectionWithRoot(target->id());
  if (!connection)
    connection = connection_manager_->GetConnection(target->id().connection_id);
  connection->client()->OnWindowInputEvent(ViewIdToTransportId(target->id()),
                                           event.Pass(),
                                           base::Bind(&base::DoNothing));
}

void ViewTreeHostImpl::SetSize(mojo::SizePtr size) {
  display_manager_->SetViewportSize(size.To<gfx::Size>());
}

void ViewTreeHostImpl::SetTitle(const mojo::String& title) {
  display_manager_->SetTitle(title.To<base::string16>());
}

void ViewTreeHostImpl::AddAccelerator(uint32_t id,
                                      mojo::KeyboardCode keyboard_code,
                                      mojo::EventFlags flags) {
  event_dispatcher_.AddAccelerator(id, keyboard_code, flags);
}

void ViewTreeHostImpl::RemoveAccelerator(uint32_t id) {
  event_dispatcher_.RemoveAccelerator(id);
}

void ViewTreeHostImpl::OnClientClosed() {
  // |display_manager_.reset()| destroys the display-manager first, and then
  // sets |display_manager_| to nullptr. However, destroying |display_manager_|
  // can destroy the corresponding ViewTreeHostConnection, and |this|. So
  // setting it to nullptr afterwards in reset() ends up writing on free'd
  // memory. So transfer over to a local scoped_ptr<> before destroying it.
  scoped_ptr<DisplayManager> temp = display_manager_.Pass();
}

ServerView* ViewTreeHostImpl::GetRootView() {
  return root_.get();
}

void ViewTreeHostImpl::OnEvent(mojo::EventPtr event) {
  event_dispatcher_.OnEvent(event.Pass());
}

void ViewTreeHostImpl::OnDisplayClosed() {
  if (delegate_)
    delegate_->OnDisplayClosed();
}

void ViewTreeHostImpl::OnViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerView(
        RootViewId(connection_manager_->GetAndAdvanceNextHostId())));
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    root_->SetVisible(true);
    if (delegate_)
      delegate_->OnDisplayInitialized();
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
  }
  // TODO(fsamuel): We shouldn't broadcast this to all connections but only
  // those within a window root.
  connection_manager_->ProcessViewportMetricsChanged(old_metrics, new_metrics);
}

void ViewTreeHostImpl::OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) {
  surface_id_ = surface_id;
}

void ViewTreeHostImpl::OnFocusChanged(ServerView* old_focused_view,
                                      ServerView* new_focused_view) {
  // There are up to four connections that need to be notified:
  // . the connection containing |old_focused_view|.
  // . the connection with |old_focused_view| as its root.
  // . the connection containing |new_focused_view|.
  // . the connection with |new_focused_view| as its root.
  // Some of these connections may be the same. The following takes care to
  // notify each only once.
  ViewTreeImpl* owning_connection_old = nullptr;
  ViewTreeImpl* embedded_connection_old = nullptr;

  if (old_focused_view) {
    owning_connection_old = connection_manager_->GetConnection(
        old_focused_view->id().connection_id);
    if (owning_connection_old) {
      owning_connection_old->ProcessFocusChanged(old_focused_view,
                                                 new_focused_view);
    }
    embedded_connection_old =
        connection_manager_->GetConnectionWithRoot(old_focused_view->id());
    if (embedded_connection_old) {
      DCHECK_NE(owning_connection_old, embedded_connection_old);
      embedded_connection_old->ProcessFocusChanged(old_focused_view,
                                                   new_focused_view);
    }
  }
  ViewTreeImpl* owning_connection_new = nullptr;
  ViewTreeImpl* embedded_connection_new = nullptr;
  if (new_focused_view) {
    owning_connection_new = connection_manager_->GetConnection(
        new_focused_view->id().connection_id);
    if (owning_connection_new &&
        owning_connection_new != owning_connection_old &&
        owning_connection_new != embedded_connection_old) {
      owning_connection_new->ProcessFocusChanged(old_focused_view,
                                                 new_focused_view);
    }
    embedded_connection_new =
        connection_manager_->GetConnectionWithRoot(new_focused_view->id());
    if (embedded_connection_new &&
        embedded_connection_new != owning_connection_old &&
        embedded_connection_new != embedded_connection_old) {
      DCHECK_NE(owning_connection_new, embedded_connection_new);
      embedded_connection_new->ProcessFocusChanged(old_focused_view,
                                                   new_focused_view);
    }
  }

  // Ensure that we always notify the root connection of a focus change.
  ViewTreeImpl* root_tree = GetViewTree();
  if (root_tree != owning_connection_old &&
      root_tree != embedded_connection_old &&
      root_tree != owning_connection_new &&
      root_tree != embedded_connection_new) {
    root_tree->ProcessFocusChanged(old_focused_view, new_focused_view);
  }

  UpdateTextInputState(new_focused_view, new_focused_view->text_input_state());
}

}  // namespace mus
