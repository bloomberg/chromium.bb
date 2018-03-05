// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include "components/arc/common/accessibility_helper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

class AXTreeSourceArcTest : public testing::Test,
                            public AXTreeSourceArc::Delegate {
 public:
  AXTreeSourceArcTest() : tree_(new AXTreeSourceArc(this)) {}

 protected:
  void CallNotifyAccessibilityEvent(mojom::AccessibilityEventData* event_data) {
    tree_->NotifyAccessibilityEvent(event_data);
  }

  void CallGetChildren(
      mojom::AccessibilityNodeInfoData* node,
      std::vector<mojom::AccessibilityNodeInfoData*>* out_children) const {
    tree_->GetChildren(node, out_children);
  }

  void CallSerializeNode(mojom::AccessibilityNodeInfoData* node,
                         std::unique_ptr<ui::AXNodeData>* out_data) const {
    ASSERT_TRUE(out_data);
    *out_data = std::make_unique<ui::AXNodeData>();
    tree_->SerializeNode(node, out_data->get());
  }

 private:
  void OnAction(const ui::AXActionData& data) const override {}

  std::unique_ptr<AXTreeSourceArc> tree_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArcTest);
};

TEST_F(AXTreeSourceArcTest, ReorderChildrenByLayout) {

  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->source_id = 0;
  event1->task_id = 1;
  event1->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[0]->id = 0;
  event1->node_data[0]->int_list_properties =
      std::unordered_map<arc::mojom::AccessibilityIntListProperty,
                         std::vector<int>>();
  event1->node_data[0]->int_list_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityIntListProperty::CHILD_NODE_IDS,
                     std::vector<int>({1, 2})));

  // Child button.
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[1]->id = 1;
  event1->node_data[1]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();
  event1->node_data[1]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::CLASS_NAME,
                     ui::kAXButtonClassname));
  event1->node_data[1]->boolean_properties =
      std::unordered_map<arc::mojom::AccessibilityBooleanProperty, bool>();
  event1->node_data[1]->boolean_properties.value().insert(std::make_pair(
      arc::mojom::AccessibilityBooleanProperty::VISIBLE_TO_USER, true));

  // Another child button.
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[2]->id = 2;
  event1->node_data[2]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();
  event1->node_data[2]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::CLASS_NAME,
                     ui::kAXButtonClassname));
  event1->node_data[2]->boolean_properties =
      std::unordered_map<arc::mojom::AccessibilityBooleanProperty, bool>();
  event1->node_data[2]->boolean_properties.value().insert(std::make_pair(
      arc::mojom::AccessibilityBooleanProperty::VISIBLE_TO_USER, true));

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event1.get());

  // Live edit the data sources to exercise each layout.

  // Non-overlapping, bottom to top.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  std::vector<mojom::AccessibilityNodeInfoData*> top_to_bottom;
  CallGetChildren(event1->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  ASSERT_EQ(2, top_to_bottom[0]->id);
  ASSERT_EQ(1, top_to_bottom[1]->id);

  // Non-overlapping, top to bottom.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  top_to_bottom.clear();
  CallGetChildren(event1->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  ASSERT_EQ(1, top_to_bottom[0]->id);
  ASSERT_EQ(2, top_to_bottom[1]->id);

  // Overlapping; right to left.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  std::vector<mojom::AccessibilityNodeInfoData*> left_to_right;
  CallGetChildren(event1->node_data[0].get(), &left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  ASSERT_EQ(2, left_to_right[0]->id);
  ASSERT_EQ(1, left_to_right[1]->id);

  // Overlapping; left to right.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  left_to_right.clear();
  CallGetChildren(event1->node_data[0].get(), &left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  ASSERT_EQ(1, left_to_right[0]->id);
  ASSERT_EQ(2, left_to_right[1]->id);

  // Overlapping, bottom to top.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  top_to_bottom.clear();
  CallGetChildren(event1->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  ASSERT_EQ(2, top_to_bottom[0]->id);
  ASSERT_EQ(1, top_to_bottom[1]->id);

  // Overlapping, top to bottom.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  top_to_bottom.clear();
  CallGetChildren(event1->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  ASSERT_EQ(1, top_to_bottom[0]->id);
  ASSERT_EQ(2, top_to_bottom[1]->id);

  // Identical. smaller to larger.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  std::vector<mojom::AccessibilityNodeInfoData*> dimension;
  CallGetChildren(event1->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  ASSERT_EQ(2, dimension[0]->id);
  ASSERT_EQ(1, dimension[1]->id);

  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  dimension.clear();
  CallGetChildren(event1->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  ASSERT_EQ(2, dimension[0]->id);
  ASSERT_EQ(1, dimension[1]->id);

  // Identical. Larger to smaller.
  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  dimension.clear();
  CallGetChildren(event1->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  ASSERT_EQ(1, dimension[0]->id);
  ASSERT_EQ(2, dimension[1]->id);

  event1->node_data[1]->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  event1->node_data[2]->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  dimension.clear();
  CallGetChildren(event1->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  ASSERT_EQ(1, dimension[0]->id);
  ASSERT_EQ(2, dimension[1]->id);
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputation) {
  auto event = arc::mojom::AccessibilityEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event->node_data[0]->id = 0;
  event->node_data[0]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  // Live edit name related attributes.

  // No attributes.
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(event->node_data[0].get(), &data);
  std::string name;
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (empty).
  event->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::TEXT, ""));

  CallSerializeNode(event->node_data[0].get(), &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("", name);

  // Text (non-empty).
  event->node_data[0]->string_properties->clear();
  event->node_data[0]->string_properties.value().insert(std::make_pair(
      arc::mojom::AccessibilityStringProperty::TEXT, "label text"));

  CallSerializeNode(event->node_data[0].get(), &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("label text", name);

  // Content description (empty), text (non-empty).
  event->node_data[0]->string_properties.value().insert(std::make_pair(
      arc::mojom::AccessibilityStringProperty::CONTENT_DESCRIPTION, ""));

  CallSerializeNode(event->node_data[0].get(), &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("label text", name);

  // Content description (non-empty), text (non-empty).
  event->node_data[0]
      ->string_properties
      .value()[arc::mojom::AccessibilityStringProperty::CONTENT_DESCRIPTION] =
      "label content description";

  CallSerializeNode(event->node_data[0].get(), &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("label content description", name);
}

}  // namespace arc
