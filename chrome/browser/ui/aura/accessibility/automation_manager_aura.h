// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/automation_internal/automation_action_adapter.h"
#include "chrome/browser/ui/aura/accessibility/ax_tree_source_aura.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/views/focus/widget_focus_manager.h"

template <typename T>
struct DefaultSingletonTraits;

namespace content {
class BrowserContext;
}  // namespace content

namespace views {
class AXAuraObjWrapper;
class View;
}  // namespace views

using AuraAXTreeSerializer =
    ui::AXTreeSerializer<views::AXAuraObjWrapper*, ui::AXNodeData>;

// Manages a tree of automation nodes.
class AutomationManagerAura : public extensions::AutomationActionAdapter,
                              public views::WidgetFocusChangeListener {
 public:
  // Get the single instance of this class.
  static AutomationManagerAura* GetInstance();

  // Enable automation support for views.
  void Enable(content::BrowserContext* context);

  // Disable automation support for views.
  void Disable();

  // Handle an event fired upon a |View|.
  void HandleEvent(content::BrowserContext* context,
                   views::View* view,
                   ui::AXEvent event_type);

  void HandleAlert(content::BrowserContext* context, const std::string& text);

  // AutomationActionAdapter implementation.
  void DoDefault(int32 id) override;
  void Focus(int32 id) override;
  void MakeVisible(int32 id) override;
  void SetSelection(int32 id, int32 start, int32 end) override;
  void ShowContextMenu(int32 id) override;

 protected:
  ~AutomationManagerAura() override;

 private:
  friend struct DefaultSingletonTraits<AutomationManagerAura>;

  AutomationManagerAura();

  // Reset all state in this manager.
  void ResetSerializer();

  void SendEvent(content::BrowserContext* context,
                 views::AXAuraObjWrapper* aura_obj,
                 ui::AXEvent event_type);

  // views::WidgetFocusChangeListener:
  void OnNativeFocusChanged(aura::Window* focused_now) override;

  // Whether automation support for views is enabled.
  bool enabled_;

  // Holds the active views-based accessibility tree. A tree currently consists
  // of all views descendant to a |Widget| (see |AXTreeSourceViews|).
  // A tree becomes active when an event is fired on a descendant view.
  scoped_ptr<AXTreeSourceAura> current_tree_;

  // Serializes incremental updates on the currently active tree
  // |current_tree_|.
  scoped_ptr<AuraAXTreeSerializer> current_tree_serializer_;

  bool processing_events_;

  std::vector<std::pair<views::AXAuraObjWrapper*, ui::AXEvent>> pending_events_;

  aura::Window* focused_window_;

  DISALLOW_COPY_AND_ASSIGN(AutomationManagerAura);
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
