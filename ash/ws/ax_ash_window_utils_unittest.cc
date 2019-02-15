// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/ax_ash_window_utils.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

using views::AXAuraObjCache;
using views::AXAuraObjWrapper;
using views::AXTreeSourceViews;
using views::MusClient;
using views::Textfield;
using views::View;
using views::Widget;

using AuraAXTreeSerializer = ui::
    AXTreeSerializer<views::AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>;

using AuraAXTreeSourceChecker =
    ui::AXTreeSourceChecker<views::AXAuraObjWrapper*,
                            ui::AXNodeData,
                            ui::AXTreeData>;

namespace ash {
namespace {

bool HasNodeWithName(ui::AXNode* node, const std::string& name) {
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return true;
  for (auto* child : node->children()) {
    if (HasNodeWithName(child, name))
      return true;
  }
  return false;
}

bool HasNodeWithValue(ui::AXNode* node, const std::string& value) {
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kValue) == value)
    return true;
  for (auto* child : node->children()) {
    if (HasNodeWithValue(child, value))
      return true;
  }
  return false;
}

class AXAshWindowUtilsTest : public SingleProcessMashTestBase {
 public:
  AXAshWindowUtilsTest() = default;
  ~AXAshWindowUtilsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    SingleProcessMashTestBase::SetUp();

    // Create a widget with a child view.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params;
    params.name = "Test Widget";
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.native_widget =
        MusClient::Get()->CreateNativeWidget(params, widget_.get());
    widget_->Init(params);
    widget_->Show();

    textfield_ = new Textfield();
    textfield_->SetText(base::ASCIIToUTF16("Test Textfield"));
    widget_->GetContentsView()->AddChildView(textfield_);

    // Flush all messages from the WindowTreeClient to ensure the window service
    // has finished Widget creation.
    aura::test::WaitForAllChangesToComplete();
  }

  void TearDown() override {
    widget_.reset();
    SingleProcessMashTestBase::TearDown();
  }

 protected:
  std::unique_ptr<Widget> widget_;
  Textfield* textfield_ = nullptr;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AXAshWindowUtilsTest);
};

TEST_F(AXAshWindowUtilsTest, WalkUpToAshRoot) {
  AXAshWindowUtils utils;

  // Start with a window in the client.
  aura::Window* window = widget_->GetNativeWindow();
  EXPECT_EQ(aura::Env::Mode::MUS, window->env()->mode());

  // Walk up to the root node.
  while (true) {
    aura::Window* parent = utils.GetParent(window);
    if (!parent)
      break;
    window = parent;
  }

  // Walking up "jumped the fence" into ash windows.
  EXPECT_EQ(aura::Env::Mode::LOCAL, window->env()->mode());
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->GetName(), window->GetName());
}

TEST_F(AXAshWindowUtilsTest, WalkDownToClientWindow) {
  AXAshWindowUtils utils;

  // Start with the widget's container.
  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_DefaultContainer);
  EXPECT_EQ(aura::Env::Mode::LOCAL, container->env()->mode());

  // Walking down "jumps the fence" into the client window.
  std::vector<aura::Window*> children = utils.GetChildren(container);
  ASSERT_EQ(1u, children.size());
  aura::Window* child = children[0];
  EXPECT_EQ(aura::Env::Mode::MUS, child->env()->mode());
  EXPECT_EQ(widget_.get(), utils.GetWidgetForNativeView(child));
}

// Verify integration of AXAshWindowUtils with the accessibility subsystem
// serialization code.
TEST_F(AXAshWindowUtilsTest, SerializeNodeTree) {
  // Build a desktop tree serializer similar to AutomationManagerAura.
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  cache->OnRootWindowObjCreated(Shell::GetPrimaryRootWindow());
  AXRootObjWrapper root_wrapper(nullptr /* delegate */);
  AXTreeSourceViews ax_tree_source(&root_wrapper,
                                   ui::AXTreeID::CreateNewAXTreeID());
  AuraAXTreeSerializer ax_serializer(&ax_tree_source);

  // Initial tree is valid.
  AuraAXTreeSourceChecker checker(&ax_tree_source);
  ASSERT_TRUE(checker.Check());

  // Simulate initial serialization.
  ui::AXTreeUpdate initial_update;
  ax_serializer.SerializeChanges(ax_tree_source.GetRoot(), &initial_update);

  // Tree includes system UI.
  ui::AXTree ax_tree(initial_update);
  EXPECT_TRUE(HasNodeWithName(ax_tree.root(), "Shelf"));

  // Remove the child view and re-add it, which should fire some events.
  widget_->GetContentsView()->RemoveAllChildViews(false /* delete_children */);
  widget_->GetContentsView()->AddChildView(textfield_);

  // Serialize walking up from the textfield.
  ui::AXTreeUpdate label_update;
  AXAuraObjWrapper* wrapper =
      AXAuraObjCache::GetInstance()->GetOrCreate(textfield_);
  ASSERT_TRUE(ax_serializer.SerializeChanges(wrapper, &label_update));
  ASSERT_TRUE(ax_tree.Unserialize(label_update));

  // Tree still includes system UI.
  EXPECT_TRUE(HasNodeWithName(ax_tree.root(), "Shelf"));

  // The serializer also "jumped the fence" from the ash tree to the mus-client
  // tree, so the tree has nodes from inside the Widget.
  EXPECT_TRUE(HasNodeWithValue(ax_tree.root(), "Test Textfield"));

  // AXAuraObjCache can reach into a client Widget to find a focused view.
  textfield_->RequestFocus();
  EXPECT_TRUE(textfield_->HasFocus());
  AXAuraObjWrapper* textfield_wrapper = cache->GetOrCreate(textfield_);
  EXPECT_EQ(textfield_wrapper, cache->GetFocus());
}

TEST_F(AXAshWindowUtilsTest, IsRootWindow) {
  AXAshWindowUtils utils;

  // From the client's perspective a widget's root window is a root.
  aura::Window* widget_root = widget_->GetNativeWindow()->GetRootWindow();
  EXPECT_TRUE(widget_root->IsRootWindow());

  // Simulate an embedded remote client window.
  aura::WindowTreeHostMus window_tree_host(aura::CreateInitParamsForTopLevel(
      views::MusClient::Get()->window_tree_client()));
  window_tree_host.InitHost();
  window_tree_host.SetBounds(gfx::Rect(0, 0, 100, 200),
                             viz::LocalSurfaceIdAllocation());
  aura::Window* embed_root = window_tree_host.window();

  // From the client's perspective the embed is a root.
  EXPECT_TRUE(embed_root->IsRootWindow());

  // Accessibility serialization only considers display roots to be roots.
  EXPECT_TRUE(utils.IsRootWindow(Shell::GetPrimaryRootWindow()));
  EXPECT_FALSE(utils.IsRootWindow(embed_root));
  EXPECT_FALSE(utils.IsRootWindow(widget_root));
}

}  // namespace
}  // namespace ash
