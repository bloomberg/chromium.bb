// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_H_

#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// Context in which exclusive access operation is being performed. This
// interface is implemented once in Browser context and in Platform Application
// context.
class ExclusiveAccessContext {
 public:
  virtual ~ExclusiveAccessContext() {}

  // Returns the current profile associated with the window.
  virtual Profile* GetProfile() = 0;

  // Returns true if the window hosting the exclusive access bubble is
  // fullscreen.
  virtual bool IsFullscreen() const = 0;

  // Returns true if fullscreen with toolbar is supported.
  virtual bool SupportsFullscreenWithToolbar() const;

  // Shows or hides the tab strip, toolbar and bookmark bar with in browser
  // fullscreen.
  // Currently only supported on Mac.
  virtual void UpdateFullscreenWithToolbar(bool with_toolbar);

  // Toggles the toolbar state to be hidden or shown in fullscreen. Updates
  // the preference accordingly. Only supported on Mac.
  virtual void ToggleFullscreenToolbar();

  // Returns true if the window is fullscreen with additional UI elements. See
  // EnterFullscreen |with_toolbar|.
  virtual bool IsFullscreenWithToolbar() const;

  // Enters fullscreen and update exit bubble.
  // On Mac, the tab strip and toolbar will be shown if |with_toolbar| is true,
  // |with_toolbar| is ignored on other platforms.
  virtual void EnterFullscreen(const GURL& url,
                               ExclusiveAccessBubbleType bubble_type,
                               bool with_toolbar) = 0;

  // Exits fullscreen and update exit bubble.
  virtual void ExitFullscreen() = 0;

  // Updates the content of exclusive access exit bubble content.
  virtual void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) = 0;

  // Informs the exclusive access system of some user input, which may update
  // internal timers and/or re-display the bubble.
  virtual void OnExclusiveAccessUserInput() = 0;

#if defined(OS_WIN)
  // Sets state for entering or exiting Win8 Metro snap mode.
  virtual void SetMetroSnapMode(bool enable);

  // Returns whether the window is currently in Win8 Metro snap mode.
  virtual bool IsInMetroSnapMode() const;
#endif  // defined(OS_WIN)

  // Returns the currently active WebContents, or nullptr if there is none.
  virtual content::WebContents* GetActiveWebContents() = 0;

  // TODO(sriramsr): This is abstraction violation as the following two function
  // does not apply to a platform app window. Ideally, the BrowserView should
  // hide/unhide its download shelf widget when it is instructed to enter/exit
  // fullscreen mode.
  // Displays the download shelf associated with currently active window.
  virtual void UnhideDownloadShelf() = 0;

  // Hides download shelf associated with currently active window.
  virtual void HideDownloadShelf() = 0;
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_H_
