// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"

@class MenuController;

// Mac implementation of the context menu display code. Uses a Cocoa NSMenu
// to display the context menu. Internally uses an obj-c object as the
// target of the NSMenu, bridging back to this C++ class.
class RenderViewContextMenuMac : public RenderViewContextMenu {
 public:
  RenderViewContextMenuMac(content::WebContents* web_contents,
                           const content::ContextMenuParams& params,
                           NSView* parent_view);
  virtual ~RenderViewContextMenuMac();

  // SimpleMenuModel::Delegate implementation.
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;

  // RenderViewContextMenuDelegate implementation.
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) OVERRIDE;

 protected:
  // RenderViewContextMenu implementation.
  virtual void PlatformInit() OVERRIDE;
  virtual void PlatformCancel() OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  // Adds platform-specific items to the menu.
  void InitPlatformMenu();

  // Handler for the "Look Up in Dictionary" menu item.
  void LookUpInDictionary();

  // Handler for the "Start Speaking" menu item.
  void StartSpeaking();

  // Handler for the "Stop Speaking" menu item.
  void StopSpeaking();

  // The Cocoa menu controller for this menu.
  scoped_nsobject<MenuController> menu_controller_;

  // Model for the "Speech" submenu.
  ui::SimpleMenuModel speech_submenu_model_;

  // The Cocoa parent view.
  NSView* parent_view_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuMac);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_
