// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_impl.h"

#include "components/mus/example/wm/container.h"
#include "components/mus/example/wm/window_manager_application.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"

namespace {

mus::Id GetViewIdForContainer(mus::ViewTreeConnection* connection,
                              Container container) {
  return connection->GetConnectionId() << 16 | static_cast<uint16>(container);
}

}  // namespace

WindowManagerImpl::WindowManagerImpl(
    WindowManagerApplication* state,
    mojo::InterfaceRequest<mus::mojom::WindowManager> request)
    : state_(state),
      binding_(this, request.Pass()) {
}

WindowManagerImpl::~WindowManagerImpl() {}

void WindowManagerImpl::OpenWindow(mojo::ViewTreeClientPtr client) {
  mus::View* root = state_->root();
  DCHECK(root);
  mus::Id container_view_id = GetViewIdForContainer(root->connection(),
                                                    Container::USER_WINDOWS);
  mus::View* container_view = root->GetChildById(container_view_id);

  const int width = (root->bounds().width - 240);
  const int height = (root->bounds().height - 240);

  mus::View* child_view = root->connection()->CreateView();
  mojo::Rect bounds;
  bounds.x = 40 + (state_->window_count() % 4) * 40;
  bounds.y = 40 + (state_->window_count() % 4) * 40;
  bounds.width = width;
  bounds.height = height;
  child_view->SetBounds(bounds);
  container_view->AddChild(child_view);
  child_view->Embed(client.Pass());

  state_->IncrementWindowCount();
}

void WindowManagerImpl::CenterWindow(uint32_t view_id, mojo::SizePtr size) {
  // TODO(beng):
}

