// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_H_

#include "base/macros.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"

namespace mus {

// Testing WindowTree implementation.
class TestWindowTree : public mojom::WindowTree {
 public:
  TestWindowTree();
  ~TestWindowTree() override;

  // Returns the most recent change_id supplied to one of the WindowTree
  // functions. Returns false if one of the WindowTree functions has not been
  // invoked since the last GetAndClearChangeId().
  bool GetAndClearChangeId(uint32_t* change_id);

 private:
  // mojom::WindowTree:
  void NewWindow(uint32_t change_id, uint32_t window_id) override;
  void DeleteWindow(uint32_t window_id,
                    const DeleteWindowCallback& callback) override;
  void SetWindowBounds(uint32_t change_id,
                       uint32_t window_id,
                       mojo::RectPtr bounds) override;
  void SetClientArea(uint32_t window_id, mojo::InsetsPtr insets) override;
  void SetWindowVisibility(
      uint32_t window_id,
      bool visible,
      const SetWindowVisibilityCallback& callback) override;
  void SetWindowProperty(uint32_t change_id,
                         uint32_t window_id,
                         const mojo::String& name,
                         mojo::Array<uint8_t> value) override;
  void RequestSurface(uint32_t window_id,
                      mojom::SurfaceType type,
                      mojo::InterfaceRequest<mojom::Surface> surface,
                      mojom::SurfaceClientPtr client) override;
  void AddWindow(uint32_t parent,
                 uint32_t child,
                 const AddWindowCallback& callback) override;
  void RemoveWindowFromParent(
      uint32_t window_id,
      const RemoveWindowFromParentCallback& callback) override;
  void AddTransientWindow(uint32_t change_id,
                          uint32_t window_id,
                          uint32_t transient_window_id) override;
  void RemoveTransientWindowFromParent(uint32_t change_id,
                                       uint32_t window_id) override;
  void ReorderWindow(uint32_t window_id,
                     uint32_t relative_window_id,
                     mojom::OrderDirection direction,
                     const ReorderWindowCallback& callback) override;
  void GetWindowTree(uint32_t window_id,
                     const GetWindowTreeCallback& callback) override;
  void Embed(uint32_t window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t policy_bitmask,
             const EmbedCallback& callback) override;
  void SetFocus(uint32_t window_id) override;
  void SetWindowTextInputState(uint32_t window_id,
                               mojo::TextInputStatePtr state) override;
  void SetImeVisibility(uint32_t window_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;
  void SetPreferredSize(uint32_t window_id,
                        mojo::SizePtr size,
                        const SetPreferredSizeCallback& callback) override;
  void SetShowState(uint32_t window_id,
                    mojom::ShowState show_state,
                    const SetShowStateCallback& callback) override;
  void SetResizeBehavior(uint32_t window_id,
                         mojom::ResizeBehavior resize_behavior) override;
  void WmResponse(uint32_t change_id, bool response) override;

  bool got_change_;
  uint32_t change_id_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTree);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_H_
