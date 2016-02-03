// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_EXCLUSIVE_ACCESS_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_EXCLUSIVE_ACCESS_CONTROLLER_VIEWS_H_

// Note this file has a _views suffix so that it may have an optional runtime
// dependency on toolkit-views UI.

#import <CoreGraphics/CoreGraphics.h>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "ui/base/accelerators/accelerator.h"

class Browser;
class BrowserWindow;
@class BrowserWindowController;
class ExclusiveAccessBubbleViews;
@class ExclusiveAccessBubbleWindowController;
class GURL;

// Component placed into a browser window controller to manage communication
// with the exclusive access bubble, which appears for events such as entering
// fullscreen.
class ExclusiveAccessController : public ExclusiveAccessContext,
                                  public ui::AcceleratorProvider,
                                  public ExclusiveAccessBubbleViewsContext {
 public:
  ExclusiveAccessController(BrowserWindowController* controller,
                            Browser* browser);
  ~ExclusiveAccessController() override;

  const GURL& url() const { return url_; }
  ExclusiveAccessBubbleType bubble_type() const { return bubble_type_; }
  ExclusiveAccessBubbleWindowController* cocoa_bubble() {
    return cocoa_bubble_;
  }

  // Shows the bubble once the NSWindow has received -windowDidEnterFullScreen:.
  void Show();

  // Closes any open bubble.
  void Destroy();

  // If showing, position the bubble at the given y-coordinate.
  void Layout(CGFloat max_y);

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  bool IsFullscreen() const override;
  bool SupportsFullscreenWithToolbar() const override;
  void UpdateFullscreenWithToolbar(bool with_toolbar) override;
  void ToggleFullscreenToolbar() override;
  bool IsFullscreenWithToolbar() const override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type,
                       bool with_toolbar) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetActiveWebContents() override;
  void UnhideDownloadShelf() override;
  void HideDownloadShelf() override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  views::Widget* GetBubbleAssociatedWidget() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Point GetCursorPointInParent() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() override;
  gfx::Rect GetTopContainerBoundsInScreen() override;

 private:
  BrowserWindow* GetBrowserWindow() const;

  BrowserWindowController* controller_;  // Weak. Owns |this|.
  Browser* browser_;                     // Weak. Owned by controller.

  // When going fullscreen for a tab, we need to store the URL and the
  // fullscreen type, since we can't show the bubble until
  // -windowDidEnterFullScreen: gets called.
  GURL url_;
  ExclusiveAccessBubbleType bubble_type_;

  scoped_ptr<ExclusiveAccessBubbleViews> views_bubble_;
  base::scoped_nsobject<ExclusiveAccessBubbleWindowController> cocoa_bubble_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessController);
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_EXCLUSIVE_ACCESS_CONTROLLER_VIEWS_H_
