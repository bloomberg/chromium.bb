// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame.h"

#include <algorithm>

#include "base/stl_util.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_property.h"
#include "mandoline/tab/frame_tree.h"

using mojo::View;

DECLARE_VIEW_PROPERTY_TYPE(mandoline::Frame*);

namespace mandoline {

// Used to find the Frame associated with a View.
DEFINE_LOCAL_VIEW_PROPERTY_KEY(Frame*, kFrame, nullptr);

Frame::Frame(FrameTree* tree,
             View* view,
             ViewOwnership view_ownership,
             mojo::InterfaceRequest<mojo::ServiceProvider>* services,
             mojo::ServiceProviderPtr* exposed_services)
    : tree_(tree),
      view_(view),
      parent_(nullptr),
      view_ownership_(view_ownership) {
  view_->SetLocalProperty(kFrame, this);
  view_->AddObserver(this);

  if (services)
    *services = GetProxy(&services_).Pass();

  if (exposed_services) {
    mojo::ServiceProviderPtr exposed_services_ptr;
    exposed_services_.Bind(GetProxy(&exposed_services_ptr));
    *exposed_services = exposed_services_ptr.Pass();
  }
}

Frame::~Frame() {
  if (view_)
    view_->RemoveObserver(this);
  STLDeleteElements(&children_);
  if (parent_)
    parent_->Remove(this);
  if (view_)
    view_->ClearLocalProperty(kFrame);
  if (view_ownership_ == ViewOwnership::OWNS_VIEW)
    view_->Destroy();
}

// static
Frame* Frame::FindFirstFrameAncestor(View* view) {
  while (view && !view->GetLocalProperty(kFrame))
    view = view->parent();
  return view ? view->GetLocalProperty(kFrame) : nullptr;
}

void Frame::Add(Frame* node) {
  if (node->parent_)
    node->parent_->Remove(node);

  node->parent_ = this;
  children_.push_back(node);
}

void Frame::Remove(Frame* node) {
  DCHECK_EQ(node->parent_, this);
  auto iter = std::find(children_.begin(), children_.end(), node);
  DCHECK(iter != children_.end());
  node->parent_ = nullptr;
  children_.erase(iter);
}

void Frame::OnViewDestroying(mojo::View* view) {
  if (parent_)
    parent_->Remove(this);

  // Reset |view_ownership_| so we don't attempt to delete |view_| in the
  // destructor.
  view_ownership_ = ViewOwnership::DOESNT_OWN_VIEW;

  // Assume the view associated with the root is never deleted out from under
  // us.
  // TODO(sky): Change browser to create a child for each FrameTree.
  if (tree_->root() == this) {
    view_->RemoveObserver(this);
    view_ = nullptr;
    return;
  }

  delete this;
}

}  // namespace mandoline
