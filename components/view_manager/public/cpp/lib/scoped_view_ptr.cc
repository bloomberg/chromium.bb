// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/scoped_view_ptr.h"

#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_observer.h"

namespace mojo {

ScopedViewPtr::ScopedViewPtr(View* view)
    : view_(view) {
  view_->AddObserver(this);
}

ScopedViewPtr::~ScopedViewPtr() {
  if (view_)
    DeleteViewOrViewManager(view_);
  DetachFromView();
}

// static
void ScopedViewPtr::DeleteViewOrViewManager(View* view) {
  if (view->view_manager()->GetRoot() == view)
    delete view->view_manager();
  else
    view->Destroy();
}

void ScopedViewPtr::DetachFromView() {
  if (!view_)
    return;

  view_->RemoveObserver(this);
  view_ = nullptr;
}

void ScopedViewPtr::OnViewDestroying(View* view) {
  DCHECK_EQ(view_, view);
  DetachFromView();
}

}  // namespace mojo
