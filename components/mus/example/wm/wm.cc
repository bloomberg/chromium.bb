// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/wm.h"

#include "components/mus/example/wm/container.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"

namespace {

mus::Id GetViewIdForContainer(mus::ViewTreeConnection* connection,
                              Container container) {
  return connection->GetConnectionId() << 16 | static_cast<uint16>(container);
}

}  // namespace

WM::WM() : root_(nullptr), window_counter_(0) {}

WM::~WM() {
  // TODO(sky): shutdown if necessary.
}

void WM::OnEmbed(mus::View* root) {
  root_ = root;
  CreateContainers();
  for (mojo::ViewTreeClientPtr* client : pending_embeds_)
    OpenWindow(client->Pass());
  pending_embeds_.clear();
}

void WM::OnConnectionLost(mus::ViewTreeConnection* connection) {
  // TODO(sky): shutdown.
  NOTIMPLEMENTED();
}

void WM::OpenWindow(mojo::ViewTreeClientPtr client) {
  if (!root_) {
    pending_embeds_.push_back(new mojo::ViewTreeClientPtr(client.Pass()));
    return;
  }

  mus::Id container_view_id = GetViewIdForContainer(root_->connection(),
                                                    Container::USER_WINDOWS);
  mus::View* container_view = root_->GetChildById(container_view_id);

  const int width = (root_->bounds().width - 240);
  const int height = (root_->bounds().height - 240);

  mus::View* child_view = root_->connection()->CreateView();
  mojo::Rect bounds;
  bounds.x = 40 + (window_counter_ % 4) * 40;
  bounds.y = 40 + (window_counter_ % 4) * 40;
  bounds.width = width;
  bounds.height = height;
  child_view->SetBounds(bounds);
  container_view->AddChild(child_view);
  child_view->Embed(client.Pass());

  window_counter_++;
}

void WM::CreateContainers() {
  for (uint16 container = static_cast<uint16>(Container::ALL_USER_BACKGROUND);
       container < static_cast<uint16>(Container::COUNT); ++container) {
    mus::View* view = root_->connection()->CreateView();
    DCHECK_EQ(mus::LoWord(view->id()), container)
        << "Containers must be created before other views!";
    view->SetBounds(root_->bounds());
    view->SetVisible(true);
    root_->AddChild(view);
  }
}

