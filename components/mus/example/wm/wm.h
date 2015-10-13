// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WM_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WM_H_

#include "base/memory/scoped_vector.h"
#include "components/mus/example/wm/wm.mojom.h"
#include "components/mus/public/cpp/view_tree_delegate.h"

class WM : public mojom::WM, public mus::ViewTreeDelegate {
 public:
  WM();
  ~WM() override;

 private:
  // ViewTreeDelegate:
  void OnEmbed(mus::View* root) override;
  void OnConnectionLost(mus::ViewTreeConnection* connection) override;

  // WM:
  void Embed(mojo::ViewTreeClientPtr client) override;

  mus::View* root_;
  ScopedVector<mojo::ViewTreeClientPtr> pending_embeds_;
  int window_counter_;

  DISALLOW_COPY_AND_ASSIGN(WM);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WM_H_
