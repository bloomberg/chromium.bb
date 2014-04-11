// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_

#include "base/memory/scoped_ptr.h"

class RenderViewContextMenu;

namespace content {
class WebContents;
struct ContextMenuParams;
}

// A ContextMenuDelegate can build and show renderer context menu.
class ContextMenuDelegate {
 public:
  explicit ContextMenuDelegate(content::WebContents* web_contents);
  virtual ~ContextMenuDelegate();

  static ContextMenuDelegate* FromWebContents(
      content::WebContents* web_contents);

  // Builds and returns a context menu for a context specified by |params|.
  // The returned value can be used to display the context menu.
  virtual scoped_ptr<RenderViewContextMenu> BuildMenu(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params) = 0;

  // Displays the context menu.
  virtual void ShowMenu(scoped_ptr<RenderViewContextMenu> menu) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextMenuDelegate);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_
