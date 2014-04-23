// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view.h"

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/services/view_manager/view_delegate.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::services::view_manager::View*);

namespace mojo {
namespace services {
namespace view_manager {

DEFINE_WINDOW_PROPERTY_KEY(View*, kViewKey, NULL);

View::View(ViewDelegate* delegate, const int32_t id)
    : delegate_(delegate),
      id_(id),
      window_(NULL) {
  DCHECK(delegate);  // Must provide a delegate.
  window_.set_owned_by_parent(false);
  window_.AddObserver(this);
  window_.SetProperty(kViewKey, this);
}

View::~View() {
}

View* View::GetParent() {
  if (!window_.parent())
    return NULL;
  return window_.parent()->GetProperty(kViewKey);
}

void View::Add(View* child) {
  window_.AddChild(&child->window_);
}

void View::Remove(View* child) {
  window_.RemoveChild(&child->window_);
}

ViewId View::GetViewId() const {
  return delegate_->GetViewId(this);
}

void View::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (params.target != &window_ || params.receiver != &window_)
    return;
  AllocationScope scope;
  ViewId new_parent_id;
  if (params.new_parent)
    new_parent_id = params.new_parent->GetProperty(kViewKey)->GetViewId();
  ViewId old_parent_id;
  if (params.old_parent)
    old_parent_id = params.old_parent->GetProperty(kViewKey)->GetViewId();
  delegate_->OnViewHierarchyChanged(delegate_->GetViewId(this), new_parent_id,
                                    old_parent_id);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
