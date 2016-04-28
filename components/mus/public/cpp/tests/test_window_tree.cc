// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/test_window_tree.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mus {

TestWindowTree::TestWindowTree()
    : got_change_(false), change_id_(0), window_id_(0u) {}

TestWindowTree::~TestWindowTree() {}

bool TestWindowTree::GetAndClearChangeId(uint32_t* change_id) {
  if (!got_change_)
    return false;

  if (change_id)
    *change_id = change_id_;
  got_change_ = false;
  return true;
}

bool TestWindowTree::WasEventAcked(uint32_t event_id) const {
  return acked_events_.count(event_id);
}

void TestWindowTree::NewWindow(
    uint32_t change_id,
    uint32_t window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) {}

void TestWindowTree::NewTopLevelWindow(
    uint32_t change_id,
    uint32_t window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) {
  got_change_ = true;
  change_id_ = change_id;
  window_id_ = window_id;
}

void TestWindowTree::DeleteWindow(uint32_t change_id, uint32_t window_id) {}

void TestWindowTree::SetWindowBounds(uint32_t change_id,
                                     uint32_t window_id,
                                     mojo::RectPtr bounds) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::SetClientArea(
    uint32_t window_id,
    mojo::InsetsPtr insets,
    mojo::Array<mojo::RectPtr> additional_client_areas) {}

void TestWindowTree::SetWindowVisibility(uint32_t change_id,
                                         uint32_t window_id,
                                         bool visible) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::SetWindowProperty(uint32_t change_id,
                                       uint32_t window_id,
                                       const mojo::String& name,
                                       mojo::Array<uint8_t> value) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::SetWindowOpacity(uint32_t change_id,
                                      uint32_t window_id,
                                      float opacity) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::AttachSurface(
    uint32_t window_id,
    mojom::SurfaceType type,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {}

void TestWindowTree::AddWindow(uint32_t change_id,
                               uint32_t parent,
                               uint32_t child) {}

void TestWindowTree::RemoveWindowFromParent(uint32_t change_id,
                                            uint32_t window_id) {}

void TestWindowTree::AddTransientWindow(uint32_t change_id,
                                        uint32_t window_id,
                                        uint32_t transient_window_id) {}

void TestWindowTree::RemoveTransientWindowFromParent(
    uint32_t change_id,
    uint32_t transient_window_id) {}

void TestWindowTree::SetModal(uint32_t change_id, uint32_t window_id) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::ReorderWindow(uint32_t change_id,
                                   uint32_t window_id,
                                   uint32_t relative_window_id,
                                   mojom::OrderDirection direction) {}

void TestWindowTree::GetWindowTree(uint32_t window_id,
                                   const GetWindowTreeCallback& callback) {}

void TestWindowTree::SetCapture(uint32_t change_id, uint32_t window_id) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::ReleaseCapture(uint32_t change_id, uint32_t window_id) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::SetEventObserver(mojom::EventMatcherPtr matcher,
                                      uint32_t observer_id) {}

void TestWindowTree::Embed(uint32_t window_id,
                           mojom::WindowTreeClientPtr client,
                           const EmbedCallback& callback) {}

void TestWindowTree::SetFocus(uint32_t change_id, uint32_t window_id) {
  got_change_ = true;
  change_id_ = change_id;
}

void TestWindowTree::SetCanFocus(uint32_t window_id, bool can_focus) {}

void TestWindowTree::SetPredefinedCursor(uint32_t change_id,
                                         uint32_t window_id,
                                         mus::mojom::Cursor cursor_id) {}

void TestWindowTree::SetWindowTextInputState(uint32_t window_id,
                                             mojo::TextInputStatePtr state) {}

void TestWindowTree::SetImeVisibility(uint32_t window_id,
                                      bool visible,
                                      mojo::TextInputStatePtr state) {}

void TestWindowTree::OnWindowInputEventAck(uint32_t event_id,
                                           mus::mojom::EventResult result) {
  EXPECT_FALSE(acked_events_.count(event_id));
  acked_events_.insert(event_id);
}

void TestWindowTree::GetWindowManagerClient(
    mojo::AssociatedInterfaceRequest<mojom::WindowManagerClient> internal) {}

void TestWindowTree::GetCursorLocationMemory(
    const GetCursorLocationMemoryCallback& callback) {
  callback.Run(mojo::ScopedSharedBufferHandle());
}

}  // namespace mus
