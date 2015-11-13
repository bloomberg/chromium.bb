// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/test_window_tree.h"

namespace mus {

TestWindowTree::TestWindowTree() : got_change_(false), change_id_(0) {}

TestWindowTree::~TestWindowTree() {}

bool TestWindowTree::GetAndClearChangeId(uint32_t* change_id) {
  if (!got_change_)
    return false;

  if (change_id)
    *change_id = change_id_;
  got_change_ = false;
  return true;
}

void TestWindowTree::NewWindow(uint32_t change_id, uint32_t window_id) {}

void TestWindowTree::DeleteWindow(uint32_t window_id,
                                  const DeleteWindowCallback& callback) {}

void TestWindowTree::SetWindowBounds(uint32_t change_id,
                                     uint32_t window_id,
                                     mojo::RectPtr bounds) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::SetClientArea(uint32_t window_id, mojo::InsetsPtr insets) {
}

void TestWindowTree::SetWindowVisibility(
    uint32_t window_id,
    bool visible,
    const SetWindowVisibilityCallback& callback) {}

void TestWindowTree::SetWindowProperty(uint32_t change_id,
                                       uint32_t window_id,
                                       const mojo::String& name,
                                       mojo::Array<uint8_t> value) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::RequestSurface(
    uint32_t window_id,
    mojom::SurfaceType type,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {}

void TestWindowTree::AddWindow(uint32_t parent,
                               uint32_t child,
                               const AddWindowCallback& callback) {}

void TestWindowTree::RemoveWindowFromParent(
    uint32_t window_id,
    const RemoveWindowFromParentCallback& callback) {}

void TestWindowTree::AddTransientWindow(uint32_t change_id,
                                        uint32_t window_id,
                                        uint32_t transient_window_id) {}

void TestWindowTree::RemoveTransientWindowFromParent(
    uint32_t change_id,
    uint32_t transient_window_id) {}

void TestWindowTree::ReorderWindow(uint32_t window_id,
                                   uint32_t relative_window_id,
                                   mojom::OrderDirection direction,
                                   const ReorderWindowCallback& callback) {}

void TestWindowTree::GetWindowTree(uint32_t window_id,
                                   const GetWindowTreeCallback& callback) {}

void TestWindowTree::Embed(uint32_t window_id,
                           mojom::WindowTreeClientPtr client,
                           uint32_t policy_bitmask,
                           const EmbedCallback& callback) {}

void TestWindowTree::SetFocus(uint32_t window_id) {}

void TestWindowTree::SetWindowTextInputState(uint32_t window_id,
                                             mojo::TextInputStatePtr state) {}

void TestWindowTree::SetImeVisibility(uint32_t window_id,
                                      bool visible,
                                      mojo::TextInputStatePtr state) {}

void TestWindowTree::SetPreferredSize(
    uint32_t window_id,
    mojo::SizePtr size,
    const SetPreferredSizeCallback& callback) {}

void TestWindowTree::SetShowState(uint32_t window_id,
                                  mojom::ShowState show_state,
                                  const SetShowStateCallback& callback) {}

void TestWindowTree::SetResizeBehavior(uint32_t window_id,
                                       mojom::ResizeBehavior resize_behavior) {}

void TestWindowTree::WmResponse(uint32_t change_id, bool response) {}

}  // namespace mus
