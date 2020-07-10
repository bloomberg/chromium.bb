// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"

@class MenuControllerCocoa;

// Mac Cocoa implementation of the renderer context menu display code. Uses a
// NSMenu to display the context menu. Internally uses an Obj-C object as the
// target of the NSMenu, bridging back to this C++ class.
class RenderViewContextMenuMacCocoa : public RenderViewContextMenuMac {
 public:
  RenderViewContextMenuMacCocoa(content::RenderFrameHost* render_frame_host,
                                const content::ContextMenuParams& params,
                                NSView* parent_view);

  ~RenderViewContextMenuMacCocoa() override;

  // RenderViewContextMenuViewsMac:
  void Show() override;

 private:
  friend class ToolkitDelegateMacCocoa;

  // Cancels the menu.
  void CancelToolkitMenu();

  // Updates the status and text of the specified context-menu item.
  void UpdateToolkitMenuItem(int command_id,
                             bool enabled,
                             bool hidden,
                             const base::string16& title);

  // The Cocoa menu controller for this menu.
  base::scoped_nsobject<MenuControllerCocoa> menu_controller_;

  // The Cocoa parent view.
  NSView* parent_view_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuMacCocoa);
};

// The ChromeSwizzleServicesMenuUpdater filters Services menu items in the
// contextual menus and elsewhere using swizzling.
@interface ChromeSwizzleServicesMenuUpdater : NSObject
// Return filtered entries, for testing.
+ (void)storeFilteredEntriesForTestingInArray:(NSMutableArray*)array;
@end

#endif  // CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_COCOA_H_