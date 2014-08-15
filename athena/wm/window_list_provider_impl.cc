// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_list_provider_impl.h"

#include "ui/aura/window.h"

namespace athena {

WindowListProviderImpl::WindowListProviderImpl(aura::Window* container)
    : container_(container) {
  CHECK(container_);
}

WindowListProviderImpl::~WindowListProviderImpl() {
}

aura::Window::Windows WindowListProviderImpl::GetWindowList() const {
  aura::Window::Windows list;
  const aura::Window::Windows& container_children = container_->children();
  for (aura::Window::Windows::const_iterator iter = container_children.begin();
       iter != container_children.end();
       ++iter) {
    if ((*iter)->type() == ui::wm::WINDOW_TYPE_NORMAL)
      list.push_back(*iter);
  }
  return list;
}

}  // namespace athena
