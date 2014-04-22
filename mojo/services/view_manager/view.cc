// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view.h"

#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::services::view_manager::View*);

namespace mojo {
namespace services {
namespace view_manager {

DEFINE_WINDOW_PROPERTY_KEY(View*, kViewKey, NULL);

// TODO(sky): figure out window delegate.
View::View(int32_t id) : id_(id), window_(NULL) {
  window_.set_owned_by_parent(false);
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

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
