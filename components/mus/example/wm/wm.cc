// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/wm.h"

#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"

WM::WM() : root_(nullptr), window_counter_(0) {}

WM::~WM() {
  // TODO(sky): shutdown if necessary.
}

void WM::OnEmbed(mus::View* root) {
  root_ = root;
  for (mojo::ViewTreeClientPtr* client : pending_embeds_)
    Embed(client->Pass());
  pending_embeds_.clear();
}

void WM::OnConnectionLost(mus::ViewTreeConnection* connection) {
  // TODO(sky): shutdown.
  NOTIMPLEMENTED();
}

void WM::Embed(mojo::ViewTreeClientPtr client) {
  if (!root_) {
    pending_embeds_.push_back(new mojo::ViewTreeClientPtr(client.Pass()));
    return;
  }

  const int width = (root_->bounds().width - 240);
  const int height = (root_->bounds().height - 240);

  mus::View* child_view = root_->connection()->CreateView();
  mojo::Rect bounds;
  bounds.x = 40 + (window_counter_ % 4) * 40;
  bounds.y = 40 + (window_counter_ % 4) * 40;
  bounds.width = width;
  bounds.height = height;
  child_view->SetBounds(bounds);
  root_->AddChild(child_view);
  child_view->Embed(client.Pass());

  window_counter_++;
}
