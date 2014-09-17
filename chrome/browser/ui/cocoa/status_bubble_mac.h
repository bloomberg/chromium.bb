// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_BUBBLE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_BUBBLE_MAC_H_

#include <string>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/status_bubble.h"
#include "url/gurl.h"

class StatusBubbleMacTest;
@class StatusBubbleWindow;

class StatusBubbleMac : public StatusBubble {
 public:
  // The various states that a status bubble may be in.  Public for delegate
  // access (for testing).
  enum StatusBubbleState {
    kBubbleHidden,         // Fully hidden
    kBubbleShowingTimer,   // Waiting to fade in
    kBubbleShowingFadeIn,  // In a fade-in transition
    kBubbleShown,          // Fully visible
    kBubbleHidingTimer,    // Waiting to fade out
    kBubbleHidingFadeOut   // In a fade-out transition
  };

  StatusBubbleMac(NSWindow* parent, id delegate);
  virtual ~StatusBubbleMac();

  // StatusBubble implementation.
  virtual void SetStatus(const base::string16& status) OVERRIDE;
  virtual void SetURL(const GURL& url, const std::string& languages) OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void MouseMoved(const gfx::Point& location,
                          bool left_content) OVERRIDE;
  virtual void UpdateDownloadShelfVisibility(bool visible) OVERRIDE;

  // Mac-specific method: Update the size and position of the status bubble to
  // match the parent window. Safe to call even when the status bubble does not
  // exist.
  void UpdateSizeAndPosition();

  // Mac-specific method: Change the parent window of the status bubble. Safe to
  // call even when the status bubble does not exist.
  void SwitchParentWindow(NSWindow* parent);

  // Expand the bubble to fit a URL too long for the standard bubble size.
  void ExpandBubble();

 protected:
  // Get the current location of the mouse. Protected so that it can be
  // stubbed out for testing.
  virtual gfx::Point GetMouseLocation();

 private:
  friend class StatusBubbleMacTest;

  // Setter for state_.  Use this instead of writing to state_ directly so
  // that state changes can be observed by unit tests.
  void SetState(StatusBubbleState state);

  // Sets the bubble text for SetStatus and SetURL.
  void SetText(const base::string16& text, bool is_url);

  // Construct the window/widget if it does not already exist. (Safe to call if
  // it does.)
  void Create();

  // Attaches the status bubble window to its parent window. Safe to call even
  // when already attached.
  void Attach();

  // Detaches the status bubble window from its parent window.
  void Detach();

  // Is the status bubble attached to the browser window? It should be attached
  // when shown and during any fades, but should be detached when hidden.
  bool is_attached() { return [window_ parentWindow] != nil; }

  // Begins fading the status bubble window in or out depending on the value
  // of |show|.  This must be called from the appropriate fade state,
  // kBubbleShowingFadeIn or kBubbleHidingFadeOut, or from the appropriate
  // fully-shown/hidden state, kBubbleShown or kBubbleHidden.  This may be
  // called at any point during a fade-in or fade-out; it is even possible to
  // reverse a transition before it has completed.
  void Fade(bool show);

  // Starts an animation of the bubble window to a specific alpha value over a
  // specific period of time.
  void AnimateWindowAlpha(CGFloat alpha, NSTimeInterval duration);

  // Method called from the completion callbacks when a fade-in or fade-out
  // transition has completed.
  void AnimationDidStop();

  // One-shot timer operations to manage the delays associated with the
  // kBubbleShowingTimer and kBubbleHidingTimer states.  StartTimer and
  // TimerFired must be called from one of these states.  StartTimer may be
  // called while the timer is still running; in that case, the timer will be
  // reset. CancelTimer may be called from any state.
  void StartTimer(int64 time_ms);
  void CancelTimer();
  void TimerFired();

  // Begin the process of showing or hiding the status bubble.  These may be
  // called from any state, and will take the appropriate action to initiate
  // any state changes that may be needed.
  void StartShowing();
  void StartHiding();

  // Cancel the expansion timer.
  void CancelExpandTimer();

  // Sets the frame of the status bubble window to |window_frame|, adjusting
  // for the given mouse position if necessary. Protected for use in tests.
  void SetFrameAvoidingMouse(NSRect window_frame, const gfx::Point& mouse_pos);

  // Calculate the appropriate frame for the status bubble window. If
  // |expanded_width|, use entire width of parent frame.
  NSRect CalculateWindowFrame(bool expanded_width);

  // Returns the flags to be used to round the corners of the status bubble.
  // Before 10.7, windows have square bottom corners, but in 10.7, the bottom
  // corners are rounded. This method considers the bubble's placement (as
  // proposed in window_frame) relative to its parent window in determining
  // which flags to return. This function may choose to round any corner,
  // including top corners. Note that there may be other reasons that a
  // status bubble's corner may be rounded in addition to those dependent on
  // OS version, and flags will be set or unset elsewhere to address these
  // concerns.
  unsigned long OSDependentCornerFlags(NSRect window_frame);

  // The window we attach ourselves to.
  NSWindow* parent_;  // WEAK

  // The object that we query about our vertical offset for positioning.
  id delegate_;  // WEAK

  // The window we own.
  StatusBubbleWindow* window_;

  // The status text we want to display when there are no URLs to display.
  NSString* status_text_;

  // The url we want to display when there is no status text to display.
  NSString* url_text_;

  // The status bubble's current state.  Do not write to this field directly;
  // use SetState().
  StatusBubbleState state_;

  // True if operations are to be performed immediately rather than waiting
  // for delays and transitions.  Normally false, this should only be set to
  // true for testing.
  bool immediate_;

  // True if the status bubble has been expanded. If the bubble is in the
  // expanded state and encounters a new URL, change size immediately,
  // with no hover delay.
  bool is_expanded_;

  // The original, non-elided URL.
  GURL url_;

  // Needs to be passed to ElideURL if the original URL string is wider than
  // the standard bubble width.
  std::string languages_;

  // The factory used to generate weak pointers for the show and hide delay
  // timers.
  base::WeakPtrFactory<StatusBubbleMac> timer_factory_;

  // The factory used to generate weak pointers for the expansion delay timer.
  base::WeakPtrFactory<StatusBubbleMac> expand_timer_factory_;

  // The factory used to generate weak pointers for the CAAnimation completion
  // handlers.
  base::WeakPtrFactory<StatusBubbleMac> completion_handler_factory_;

  DISALLOW_COPY_AND_ASSIGN(StatusBubbleMac);
};

// Delegate interface
@interface NSObject(StatusBubbleDelegate)
// Called to query the delegate about the frame StatusBubble should position
// itself in. Frame is returned in the parent window coordinates.
- (NSRect)statusBubbleBaseFrame;

// Called from SetState to notify the delegate of state changes.
- (void)statusBubbleWillEnterState:(StatusBubbleMac::StatusBubbleState)state;
@end

#endif  // CHROME_BROWSER_UI_COCOA_STATUS_BUBBLE_MAC_H_
