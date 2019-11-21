// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_INFO_DATA_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_INFO_DATA_WRAPPER_H_

#include "components/arc/mojom/accessibility_helper.mojom.h"

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace arc {
class AXTreeSourceArc;

// AccessibilityInfoDataWrapper represents a single ARC++ node or window. This
// class can be used by AXTreeSourceArc to encapsulate ARC-side information
// which maps to a single AXNodeData.
class AccessibilityInfoDataWrapper {
 public:
  explicit AccessibilityInfoDataWrapper(AXTreeSourceArc* tree_source);
  virtual ~AccessibilityInfoDataWrapper() = default;

  // True if this AccessibilityInfoDataWrapper represents an Android node, false
  // if it represents an Android window.
  virtual bool IsNode() const = 0;

  // These getters return nullptr if the class doesn't hold the specified type
  // of data.
  virtual mojom::AccessibilityNodeInfoData* GetNode() const = 0;
  virtual mojom::AccessibilityWindowInfoData* GetWindow() const = 0;

  virtual int32_t GetId() const = 0;
  virtual const gfx::Rect GetBounds() const = 0;
  virtual bool IsVisibleToUser() const = 0;
  virtual bool CanBeAccessibilityFocused() const = 0;
  virtual void PopulateAXRole(ui::AXNodeData* out_data) const = 0;
  virtual void PopulateAXState(ui::AXNodeData* out_data) const = 0;
  virtual void Serialize(ui::AXNodeData* out_data) const = 0;
  virtual void GetChildren(
      std::vector<AccessibilityInfoDataWrapper*>* children) const = 0;

 protected:
  AXTreeSourceArc* tree_source_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_INFO_DATA_WRAPPER_H_
