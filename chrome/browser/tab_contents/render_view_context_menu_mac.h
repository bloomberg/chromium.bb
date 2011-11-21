// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  RenderViewContextMenuMac(TabContents* web_contents,
                           const ContextMenuParams& params,
                           NSView* parent_view);
  virtual ~RenderViewContextMenuMac();
  virtual void ExecuteCommand(int id) OVERRIDE;

  // RenderViewContextMenuDelegate implementation.
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) OVERRIDE;

 protected:
  // RenderViewContextMenu implementation.
  virtual void PlatformInit() OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

  virtual void LookUpInDictionary() OVERRIDE;

  void InitPlatformMenu();
 private:
  scoped_nsobject<MenuController> menuController_;
  NSView* parent_view_;  // parent view
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_MAC_H_
