// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/non_client_frame_controller.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/top_level_window_factory.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ws/test_change_tracker.h"
#include "services/ws/test_window_tree_client.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"

namespace ash {

using NonClientFrameControllerTest = AshTestBase;

TEST_F(NonClientFrameControllerTest, CallsRequestClose) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(non_client_frame_controller);
  non_client_frame_controller->GetWidget()->Close();
  // Close should not have been scheduled on the widget yet (because the request
  // goes to the remote client).
  EXPECT_FALSE(non_client_frame_controller->GetWidget()->IsClosed());
  auto* changes = GetTestWindowTreeClient()->tracker()->changes();
  ASSERT_FALSE(changes->empty());
  // The remote client should have a request to close the window.
  EXPECT_EQ("RequestClose", ws::ChangeToDescription(changes->back()));
}

TEST_F(NonClientFrameControllerTest, WindowTitle) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(non_client_frame_controller);
  EXPECT_TRUE(non_client_frame_controller->ShouldShowWindowTitle());
  EXPECT_TRUE(non_client_frame_controller->GetWindowTitle().empty());

  // Verify GetWindowTitle() mirrors window->SetTitle().
  const base::string16 title = base::ASCIIToUTF16("X");
  window->SetTitle(title);
  EXPECT_EQ(title, non_client_frame_controller->GetWindowTitle());

  // ShouldShowWindowTitle() mirrors |aura::client::kTitleShownKey|.
  window->SetProperty(aura::client::kTitleShownKey, false);
  EXPECT_FALSE(non_client_frame_controller->ShouldShowWindowTitle());
}

TEST_F(NonClientFrameControllerTest, ExposesChildTreeIdToAccessibility) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  const std::string ax_tree_id = "123";
  window->SetProperty(ui::kChildAXTreeID, new std::string(ax_tree_id));
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  views::View* contents_view = non_client_frame_controller->GetContentsView();
  ui::AXNodeData ax_node_data;
  contents_view->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(ax_tree_id, ax_node_data.GetStringAttribute(
                            ax::mojom::StringAttribute::kChildTreeId));
  EXPECT_EQ(ax::mojom::Role::kClient, ax_node_data.role);
}

}  // namespace ash
