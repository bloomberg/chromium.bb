// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_EXIT_BUBBLE_H_
#define CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_EXIT_BUBBLE_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/fullscreen/fullscreen_exit_bubble_type.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/point.h"
#include "url/gurl.h"

class Browser;

namespace gfx {
class Rect;
}

class FullscreenExitBubble : public gfx::AnimationDelegate {
 public:
  explicit FullscreenExitBubble(Browser* browser,
                                const GURL& url,
                                FullscreenExitBubbleType bubble_type);
  virtual ~FullscreenExitBubble();

 protected:
  static const int kPaddingPx;           // Amount of padding around the link
  static const int kInitialDelayMs;      // Initial time bubble remains onscreen
  static const int kIdleTimeMs;          // Time before mouse idle triggers hide
  static const int kPositionCheckHz;     // How fast to check the mouse position
  static const int kSlideInRegionHeightPx;
                                         // Height of region triggering
                                         // slide-in
  static const int kPopupTopPx;          // Space between the popup and the top
                                         // of the screen.
  static const int kSlideInDurationMs;   // Duration of slide-in animation
  static const int kSlideOutDurationMs;  // Duration of slide-out animation

  // Returns the current desirable rect for the popup window.  If
  // |ignore_animation_state| is true this returns the rect assuming the popup
  // is fully onscreen.
  virtual gfx::Rect GetPopupRect(bool ignore_animation_state) const = 0;
  virtual gfx::Point GetCursorScreenPoint() = 0;
  virtual bool WindowContainsPoint(gfx::Point pos) = 0;

  // Returns true if the window is active.
  virtual bool IsWindowActive() = 0;

  // Hides the bubble.  This is a separate function so it can be called by a
  // timer.
  virtual void Hide() = 0;

  // Shows the bubble.
  virtual void Show() = 0;

  virtual bool IsAnimating() = 0;

  // True if the mouse position can trigger sliding in the exit fullscreen
  // bubble when the bubble is hidden.
  virtual bool CanMouseTriggerSlideIn() const = 0;

  void StartWatchingMouse();
  void StopWatchingMouse();
  bool IsWatchingMouse() const;

  // Called repeatedly to get the current mouse position and animate the bubble
  // on or off the screen as appropriate.
  void CheckMousePosition();

  void ToggleFullscreen();
  // Accepts the request. Can cause FullscreenExitBubble to be deleted.
  void Accept();
  // Denys the request. Can cause FullscreenExitBubble to be deleted.
  void Cancel();

  // The following strings may change according to the content type and URL.
  base::string16 GetCurrentMessageText() const;
  base::string16 GetCurrentDenyButtonText() const;

  // The following strings never change.
  base::string16 GetAllowButtonText() const;
  base::string16 GetInstructionText() const;

  // The browser this bubble is in.
  Browser* browser_;

  // The host the bubble is for, can be empty.
  GURL url_;

  // The type of the bubble; controls e.g. which buttons to show.
  FullscreenExitBubbleType bubble_type_;

 private:
  // Timer to delay before allowing the bubble to hide after it's initially
  // shown.
  base::OneShotTimer<FullscreenExitBubble> initial_delay_;

  // Timer to see how long the mouse has been idle.
  base::OneShotTimer<FullscreenExitBubble> idle_timeout_;

  // Timer to poll the current mouse position.  We can't just listen for mouse
  // events without putting a non-empty HWND onscreen (or hooking Windows, which
  // has other problems), so instead we run a low-frequency poller to see if the
  // user has moved in or out of our show/hide regions.
  base::RepeatingTimer<FullscreenExitBubble> mouse_position_checker_;

  // The most recently seen mouse position, in screen coordinates.  Used to see
  // if the mouse has moved since our last check.
  gfx::Point last_mouse_pos_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenExitBubble);
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_EXIT_BUBBLE_H_
