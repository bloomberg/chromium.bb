// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scoped_ui_resource.h"

#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_ptr<FakeScopedUIResource> FakeScopedUIResource::Create(
    LayerTreeHost* host) {
  return make_scoped_ptr(new FakeScopedUIResource(host));
}

FakeScopedUIResource::FakeScopedUIResource(LayerTreeHost* host) {
  ResetCounters();
  bitmap_ = UIResourceBitmap::Create(
      new uint8_t[1], UIResourceBitmap::RGBA8, gfx::Size(1, 1));
  host_ = host;
  id_ = host_->CreateUIResource(this);
}

scoped_refptr<UIResourceBitmap> FakeScopedUIResource::GetBitmap(
    UIResourceId uid,
    bool resource_lost) {
  resource_create_count++;
  if (resource_lost)
    lost_resource_count++;
  return ScopedUIResource::GetBitmap(uid, resource_lost);
}

void FakeScopedUIResource::ResetCounters() {
  resource_create_count = 0;
  lost_resource_count = 0;
}

}  // namespace cc
