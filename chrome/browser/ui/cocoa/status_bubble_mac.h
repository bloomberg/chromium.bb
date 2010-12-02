// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_BUBBLE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_BUBBLE_MAC_H_
#pragma once

#include <string>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/string16.h"
#include "base/task.h"
#include "chrome/browser/ui/status_bubble.h"
#include "googleurl/src/gurl.h"

class GURL;
class StatusBubbleMacTest;

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
  virtual void SetStatus(const string16& status);
  virtual void SetURL(const GURL& url, const string16& languages);
  virtual void Hide();
  virtual void MouseMoved(const gfx::Point& location, bool left_content);
  virtual void UpdateDownloadShelfVisibility(bool visible);

  // Mac-specific method: Update the size and position of the status bubble to
  // match the parent window. Safe to call even when the status bubble does not
  // exist.
  void UpdateSizeAndPosition();

  // Mac-specific method: Change the parent window of the status bubble. Safe to
  // call even when the status bubble does not exist.
  void SwitchParentWindow(NSWindow* parent);

  // Delegate method called when a fade-in or fade-out transition has
  // completed.  This is public so that it may be visible to the CAAnimation
  // delegate, which is an Objective-C object.
  void AnimationDidStop(CAAnimation* animation, bool finished);

  // Expand the bubble to fit a URL too long for the standard bubble size.
  void ExpandBubble();

 private:
  friend class StatusBubbleMacTest;

  // Setter for state_.  Use this instead of writing to state_ directly so
  // that state changes can be observed by unit tests.
  void SetState(StatusBubbleState state);

  // Sets the bubble text for SetStatus and SetURL.
  void SetText(const string16& text, bool is_url);

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

  // The timer factory used for show and hide delay timers.
  ScopedRunnableMethodFactory<StatusBubbleMac> timer_factory_;

  // The timer factory used for the expansion delay timer.
  ScopedRunnableMethodFactory<StatusBubbleMac> expand_timer_factory_;

  // Calculate the appropriate frame for the status bubble window. If
  // |expanded_width|, use entire width of parent frame.
  NSRect CalculateWindowFrame(bool expanded_width);

  // The window we attach ourselves to.
  NSWindow* parent_;  // WEAK

  // The object that we query about our vertical offset for positioning.
  id delegate_;  // WEAK

  // The window we own.
  NSWindow* window_;

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
  string16 languages_;

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
