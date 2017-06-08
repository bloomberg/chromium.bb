// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/browser/ax_event_notification_details.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityManagerMac
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerMac(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManagerMac() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  BrowserAccessibility* GetFocus() override;

  // Implementation of BrowserAccessibilityManager.
  void NotifyAccessibilityEvent(
      BrowserAccessibilityEvent::Source source,
      ui::AXEvent event_type,
      BrowserAccessibility* node) override;

  void OnAccessibilityEvents(
      const std::vector<AXEventNotificationDetails>& details) override;

  NSView* GetParentView();

 private:
  // AXTreeDelegate methods.
  void OnTreeDataChanged(ui::AXTree* tree,
                         const ui::AXTreeData& old_tree_data,
                         const ui::AXTreeData& new_tree_data) override;
  void OnNodeDataWillChange(ui::AXTree* tree,
                            const ui::AXNodeData& old_node_data,
                            const ui::AXNodeData& new_node_data) override;
  void OnStateChanged(ui::AXTree* tree,
                      ui::AXNode* node,
                      ui::AXState state,
                      bool new_value) override;
  void OnStringAttributeChanged(ui::AXTree* tree,
                                ui::AXNode* node,
                                ui::AXStringAttribute attr,
                                const std::string& old_value,
                                const std::string& new_value) override;
  void OnIntAttributeChanged(ui::AXTree* tree,
                             ui::AXNode* node,
                             ui::AXIntAttribute attr,
                             int32_t old_value,
                             int32_t new_value) override;
  void OnFloatAttributeChanged(ui::AXTree* tree,
                               ui::AXNode* node,
                               ui::AXFloatAttribute attr,
                               float old_value,
                               float new_value) override;
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

  // Returns an autoreleased object.
  NSDictionary* GetUserInfoForSelectedTextChangedNotification();

  // Returns an autoreleased object.
  NSDictionary* GetUserInfoForValueChangedNotification(
      const BrowserAccessibilityCocoa* native_node,
      const base::string16& deleted_text,
      const base::string16& inserted_text) const;

  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  // Keeps track of any edits that have been made by the user during a tree
  // update. Used by NSAccessibilityValueChangedNotification.
  // Maps AXNode IDs to name or value attribute changes.
  std::map<int32_t, base::string16> text_edits_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerMac);
};

}

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
