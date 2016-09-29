// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_EXCLUSIVE_ACCESS_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_EXCLUSIVE_ACCESS_CONTROLLER_VIEWS_H_

// Note this file has a _views suffix so that it may have an optional runtime
// dependency on toolkit-views UI.

#import <CoreGraphics/CoreGraphics.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class Browser;
class BrowserWindow;
@class BrowserWindowController;
class ExclusiveAccessBubbleViews;
class GURL;
class NewBackShortcutBubble;

// Component placed into a browser window controller to manage communication
// with subtle notification bubbles, which appear for events such as entering
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

  // Shows the bubble once the NSWindow has received -windowDidEnterFullScreen:.
  void Show();

  // See comments on BrowserWindow::{MaybeShow,Hide}NewBackShortcutBubble().
  void MaybeShowNewBackShortcutBubble(bool forward);
  void HideNewBackShortcutBubble();

  // Closes any open bubble.
  void Destroy();

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  bool IsFullscreen() const override;
  void UpdateUIForTabFullscreen(TabFullscreenState state) override;
  void UpdateFullscreenToolbar() override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type) override;
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
                                  ui::Accelerator* accelerator) const override;

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

  std::unique_ptr<ExclusiveAccessBubbleViews> views_bubble_;

  // This class also manages the new Back shortcut bubble (which functions the
  // same way as ExclusiveAccessBubbleViews).
  std::unique_ptr<NewBackShortcutBubble> new_back_shortcut_bubble_;
  base::TimeTicks last_back_shortcut_press_time_;

  // Used to keep track of the kShowFullscreenToolbar preference.
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessController);
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_EXCLUSIVE_ACCESS_CONTROLLER_VIEWS_H_
