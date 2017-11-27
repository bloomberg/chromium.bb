// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_H_
#define CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

@class MenuControllerCocoa;

// Mac implementation of the context menu display code. Uses a Cocoa NSMenu
// to display the context menu. Internally uses an obj-c object as the
// target of the NSMenu, bridging back to this C++ class.
class RenderViewContextMenuMac : public RenderViewContextMenu {
 public:
  RenderViewContextMenuMac(content::RenderFrameHost* render_frame_host,
                           const content::ContextMenuParams& params,
                           NSView* parent_view);
  ~RenderViewContextMenuMac() override;

  // SimpleMenuModel::Delegate implementation.
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // RenderViewContextMenuBase implementation.
  void Show() override;

 protected:
  // RenderViewContextMenu implementation.
  void AppendPlatformEditableItems() override;

 private:
  friend class ToolkitDelegateMac;

  // Adds menu to the platform's toolkit.
  void InitToolkitMenu();

  // Cancels the menu.
  void CancelToolkitMenu();

  // Updates the status and text of the specified context-menu item.
  void UpdateToolkitMenuItem(int command_id,
                             bool enabled,
                             bool hidden,
                             const base::string16& title);

  // Adds writing direction submenu.
  void AppendBidiSubMenu();

  // Handler for the "Look Up in Dictionary" menu item.
  void LookUpInDictionary();

  // Handler for the "Start Speaking" menu item.
  void StartSpeaking();

  // Handler for the "Stop Speaking" menu item.
  void StopSpeaking();

  // The Cocoa menu controller for this menu.
  base::scoped_nsobject<MenuControllerCocoa> menu_controller_;

  // Model for the "Speech" submenu.
  ui::SimpleMenuModel speech_submenu_model_;

  // Model for the BiDi input submenu.
  ui::SimpleMenuModel bidi_submenu_model_;

  // The Cocoa parent view.
  NSView* parent_view_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuMac);
};

// The ChromeSwizzleServicesMenuUpdater filters Services menu items in the
// contextual menus and elsewhere using swizzling.
@interface ChromeSwizzleServicesMenuUpdater : NSObject
// Return filtered entries, for testing.
+ (void)storeFilteredEntriesForTestingInArray:(NSMutableArray*)array;
@end

#endif  // CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_H_
