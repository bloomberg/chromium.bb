// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_bubble_mac.h"

#include <limits>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_view.h"
#include "gfx/point.h"
#include "net/base/net_util.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#include "ui/base/text/text_elider.h"

namespace {

const int kWindowHeight = 18;

// The width of the bubble in relation to the width of the parent window.
const CGFloat kWindowWidthPercent = 1.0 / 3.0;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
const int kMousePadding = 20;

const int kTextPadding = 3;

// The animation key used for fade-in and fade-out transitions.
NSString* const kFadeAnimationKey = @"alphaValue";

// The status bubble's maximum opacity, when fully faded in.
const CGFloat kBubbleOpacity = 1.0;

// Delay before showing or hiding the bubble after a SetStatus or SetURL call.
const int64 kShowDelayMilliseconds = 80;
const int64 kHideDelayMilliseconds = 250;

// How long each fade should last.
const NSTimeInterval kShowFadeInDurationSeconds = 0.120;
const NSTimeInterval kHideFadeOutDurationSeconds = 0.200;

// The minimum representable time interval.  This can be used as the value
// passed to +[NSAnimationContext setDuration:] to stop an in-progress
// animation as quickly as possible.
const NSTimeInterval kMinimumTimeInterval =
    std::numeric_limits<NSTimeInterval>::min();

// How quickly the status bubble should expand, in seconds.
const CGFloat kExpansionDuration = 0.125;

}  // namespace

@interface StatusBubbleAnimationDelegate : NSObject {
 @private
  StatusBubbleMac* statusBubble_;  // weak; owns us indirectly
}

- (id)initWithStatusBubble:(StatusBubbleMac*)statusBubble;

// Invalidates this object so that no further calls will be made to
// statusBubble_.  This should be called when statusBubble_ is released, to
// prevent attempts to call into the released object.
- (void)invalidate;

// CAAnimation delegate method
- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished;
@end

@implementation StatusBubbleAnimationDelegate

- (id)initWithStatusBubble:(StatusBubbleMac*)statusBubble {
  if ((self = [super init])) {
    statusBubble_ = statusBubble;
  }

  return self;
}

- (void)invalidate {
  statusBubble_ = NULL;
}

- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished {
  if (statusBubble_)
    statusBubble_->AnimationDidStop(animation, finished ? true : false);
}

@end

StatusBubbleMac::StatusBubbleMac(NSWindow* parent, id delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(timer_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(expand_timer_factory_(this)),
      parent_(parent),
      delegate_(delegate),
      window_(nil),
      status_text_(nil),
      url_text_(nil),
      state_(kBubbleHidden),
      immediate_(false),
      is_expanded_(false) {
}

StatusBubbleMac::~StatusBubbleMac() {
  Hide();

  if (window_) {
    [[[window_ animationForKey:kFadeAnimationKey] delegate] invalidate];
    Detach();
    [window_ release];
    window_ = nil;
  }
}

void StatusBubbleMac::SetStatus(const string16& status) {
  Create();

  SetText(status, false);
}

void StatusBubbleMac::SetURL(const GURL& url, const string16& languages) {
  url_ = url;
  languages_ = languages;

  Create();

  NSRect frame = [window_ frame];

  // Reset frame size when bubble is hidden.
  if (state_ == kBubbleHidden) {
    is_expanded_ = false;
    frame.size.width = NSWidth(CalculateWindowFrame(/*expand=*/false));
    [window_ setFrame:frame display:NO];
  }

  int text_width = static_cast<int>(NSWidth(frame) -
                                    kBubbleViewTextPositionX -
                                    kTextPadding);

  // Scale from view to window coordinates before eliding URL string.
  NSSize scaled_width = NSMakeSize(text_width, 0);
  scaled_width = [[parent_ contentView] convertSize:scaled_width fromView:nil];
  text_width = static_cast<int>(scaled_width.width);
  NSFont* font = [[window_ contentView] font];
  gfx::Font font_chr(base::SysNSStringToUTF16([font fontName]),
                     [font pointSize]);

  string16 original_url_text = net::FormatUrl(url, UTF16ToUTF8(languages));
  string16 status = ui::ElideUrl(url, font_chr, text_width,
      UTF16ToWideHack(languages));

  SetText(status, true);

  // In testing, don't use animation. When ExpandBubble is tested, it is
  // called explicitly.
  if (immediate_)
    return;
  else
    CancelExpandTimer();

  // If the bubble has been expanded, the user has already hovered over a link
  // to trigger the expanded state.  Don't wait to change the bubble in this
  // case -- immediately expand or contract to fit the URL.
  if (is_expanded_ && !url.is_empty()) {
    ExpandBubble();
  } else if (original_url_text.length() > status.length()) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        expand_timer_factory_.NewRunnableMethod(
            &StatusBubbleMac::ExpandBubble), kExpandHoverDelay);
  }
}

void StatusBubbleMac::SetText(const string16& text, bool is_url) {
  // The status bubble allows the status and URL strings to be set
  // independently.  Whichever was set non-empty most recently will be the
  // value displayed.  When both are empty, the status bubble hides.

  NSString* text_ns = base::SysUTF16ToNSString(text);

  NSString** main;
  NSString** backup;

  if (is_url) {
    main = &url_text_;
    backup = &status_text_;
  } else {
    main = &status_text_;
    backup = &url_text_;
  }

  // Don't return from this function early.  It's important to make sure that
  // all calls to StartShowing and StartHiding are made, so that all delays
  // are observed properly.  Specifically, if the state is currently
  // kBubbleShowingTimer, the timer will need to be restarted even if
  // [text_ns isEqualToString:*main] is true.

  [*main autorelease];
  *main = [text_ns retain];

  bool show = true;
  if ([*main length] > 0)
    [[window_ contentView] setContent:*main];
  else if ([*backup length] > 0)
    [[window_ contentView] setContent:*backup];
  else
    show = false;

  if (show)
    StartShowing();
  else
    StartHiding();
}

void StatusBubbleMac::Hide() {
  CancelTimer();
  CancelExpandTimer();
  is_expanded_ = false;

  bool fade_out = false;
  if (state_ == kBubbleHidingFadeOut || state_ == kBubbleShowingFadeIn) {
    SetState(kBubbleHidingFadeOut);

    if (!immediate_) {
      // An animation is in progress.  Cancel it by starting a new animation.
      // Use kMinimumTimeInterval to set the opacity as rapidly as possible.
      fade_out = true;
      [NSAnimationContext beginGrouping];
      [[NSAnimationContext currentContext] setDuration:kMinimumTimeInterval];
      [[window_ animator] setAlphaValue:0.0];
      [NSAnimationContext endGrouping];
    }
  }

  if (!fade_out) {
    // No animation is in progress, so the opacity can be set directly.
    [window_ setAlphaValue:0.0];
    SetState(kBubbleHidden);
  }

  // Stop any width animation and reset the bubble size.
  if (!immediate_) {
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:kMinimumTimeInterval];
    [[window_ animator] setFrame:CalculateWindowFrame(/*expand=*/false)
                         display:NO];
    [NSAnimationContext endGrouping];
  } else {
    [window_ setFrame:CalculateWindowFrame(/*expand=*/false) display:NO];
  }

  [status_text_ release];
  status_text_ = nil;
  [url_text_ release];
  url_text_ = nil;
}

void StatusBubbleMac::MouseMoved(
    const gfx::Point& location, bool left_content) {
  if (left_content)
    return;

  if (!window_)
    return;

  // TODO(thakis): Use 'location' here instead of NSEvent.
  NSPoint cursor_location = [NSEvent mouseLocation];
  --cursor_location.y;  // docs say the y coord starts at 1 not 0; don't ask why

  // Bubble's base frame in |parent_| coordinates.
  NSRect baseFrame;
  if ([delegate_ respondsToSelector:@selector(statusBubbleBaseFrame)])
    baseFrame = [delegate_ statusBubbleBaseFrame];
  else
    baseFrame = [[parent_ contentView] frame];

  // Get the normal position of the frame.
  NSRect window_frame = [window_ frame];
  window_frame.origin = [parent_ convertBaseToScreen:baseFrame.origin];

  // Get the cursor position relative to the popup.
  cursor_location.x -= NSMaxX(window_frame);
  cursor_location.y -= NSMaxY(window_frame);


  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (cursor_location.y < kMousePadding &&
      cursor_location.x < kMousePadding) {
    int offset = kMousePadding - cursor_location.y;

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (cursor_location.x > 0) {
      offset = offset * ((kMousePadding - cursor_location.x) / kMousePadding);
    }

    bool isOnScreen = true;
    NSScreen* screen = [window_ screen];
    if (screen &&
        NSMinY([screen visibleFrame]) > NSMinY(window_frame) - offset) {
      isOnScreen = false;
    }

    // If something is shown below tab contents (devtools, download shelf etc.),
    // adjust the position to sit on top of it.
    bool isAnyShelfVisible = NSMinY(baseFrame) > 0;

    if (isOnScreen && !isAnyShelfVisible) {
      // Cap the offset and change the visual presentation of the bubble
      // depending on where it ends up (so that rounded corners square off
      // and mate to the edges of the tab content).
      if (offset >= NSHeight(window_frame)) {
        offset = NSHeight(window_frame);
        [[window_ contentView] setCornerFlags:
            kRoundedBottomLeftCorner | kRoundedBottomRightCorner];
      } else if (offset > 0) {
        [[window_ contentView] setCornerFlags:
            kRoundedTopRightCorner | kRoundedBottomLeftCorner |
            kRoundedBottomRightCorner];
      } else {
        [[window_ contentView] setCornerFlags:kRoundedTopRightCorner];
      }
      window_frame.origin.y -= offset;
    } else {
      // Cannot move the bubble down without obscuring other content.
      // Move it to the right instead.
      [[window_ contentView] setCornerFlags:kRoundedTopLeftCorner];

      // Subtract border width + bubble width.
      window_frame.origin.x += NSWidth(baseFrame) - NSWidth(window_frame);
    }
  } else {
    [[window_ contentView] setCornerFlags:kRoundedTopRightCorner];
  }

  [window_ setFrame:window_frame display:YES];
}

void StatusBubbleMac::UpdateDownloadShelfVisibility(bool visible) {
}

void StatusBubbleMac::Create() {
  if (window_)
    return;

  // TODO(avi):fix this for RTL
  NSRect window_rect = CalculateWindowFrame(/*expand=*/false);
  // initWithContentRect has origin in screen coords and size in scaled window
  // coordinates.
  window_rect.size =
      [[parent_ contentView] convertSize:window_rect.size fromView:nil];
  window_ = [[NSWindow alloc] initWithContentRect:window_rect
                                        styleMask:NSBorderlessWindowMask
                                          backing:NSBackingStoreBuffered
                                            defer:YES];
  [window_ setMovableByWindowBackground:NO];
  [window_ setBackgroundColor:[NSColor clearColor]];
  [window_ setLevel:NSNormalWindowLevel];
  [window_ setOpaque:NO];
  [window_ setHasShadow:NO];

  // We do not need to worry about the bubble outliving |parent_| because our
  // teardown sequence in BWC guarantees that |parent_| outlives the status
  // bubble and that the StatusBubble is torn down completely prior to the
  // window going away.
  scoped_nsobject<BubbleView> view(
      [[BubbleView alloc] initWithFrame:NSZeroRect themeProvider:parent_]);
  [window_ setContentView:view];

  [window_ setAlphaValue:0.0];

  // Set a delegate for the fade-in and fade-out transitions to be notified
  // when fades are complete.  The ownership model is for window_ to own
  // animation_dictionary, which owns animation, which owns
  // animation_delegate.
  CAAnimation* animation = [[window_ animationForKey:kFadeAnimationKey] copy];
  [animation autorelease];
  StatusBubbleAnimationDelegate* animation_delegate =
      [[StatusBubbleAnimationDelegate alloc] initWithStatusBubble:this];
  [animation_delegate autorelease];
  [animation setDelegate:animation_delegate];
  NSMutableDictionary* animation_dictionary =
      [NSMutableDictionary dictionaryWithDictionary:[window_ animations]];
  [animation_dictionary setObject:animation forKey:kFadeAnimationKey];
  [window_ setAnimations:animation_dictionary];

  // Don't |Attach()| since we don't know the appropriate state; let the
  // |SetState()| call do that.

  [view setCornerFlags:kRoundedTopRightCorner];
  MouseMoved(gfx::Point(), false);
}

void StatusBubbleMac::Attach() {
  // This method may be called several times during the process of creating or
  // showing a status bubble to attach the bubble to its parent window.
  if (!is_attached()) {
    [parent_ addChildWindow:window_ ordered:NSWindowAbove];
    UpdateSizeAndPosition();
  }
}

void StatusBubbleMac::Detach() {
  // This method may be called several times in the process of hiding or
  // destroying a status bubble.
  if (is_attached()) {
    // Magic setFrame: See crbug.com/58506, and codereview.chromium.org/3573014
    [window_ setFrame:CalculateWindowFrame(/*expand=*/false) display:NO];
    [parent_ removeChildWindow:window_];  // See crbug.com/28107 ...
    [window_ orderOut:nil];               // ... and crbug.com/29054.
  }
}

void StatusBubbleMac::AnimationDidStop(CAAnimation* animation, bool finished) {
  DCHECK([NSThread isMainThread]);
  DCHECK(state_ == kBubbleShowingFadeIn || state_ == kBubbleHidingFadeOut);
  DCHECK(is_attached());

  if (finished) {
    // Because of the mechanism used to interrupt animations, this is never
    // actually called with finished set to false.  If animations ever become
    // directly interruptible, the check will ensure that state_ remains
    // properly synchronized.
    if (state_ == kBubbleShowingFadeIn) {
      DCHECK_EQ([[window_ animator] alphaValue], kBubbleOpacity);
      SetState(kBubbleShown);
    } else {
      DCHECK_EQ([[window_ animator] alphaValue], 0.0);
      SetState(kBubbleHidden);
    }
  }
}

void StatusBubbleMac::SetState(StatusBubbleState state) {
  // We must be hidden or attached, but not both.
  DCHECK((state_ == kBubbleHidden) ^ is_attached());

  if (state == state_)
    return;

  if (state == kBubbleHidden)
    Detach();
  else
    Attach();

  if ([delegate_ respondsToSelector:@selector(statusBubbleWillEnterState:)])
    [delegate_ statusBubbleWillEnterState:state];

  state_ = state;
}

void StatusBubbleMac::Fade(bool show) {
  DCHECK([NSThread isMainThread]);

  StatusBubbleState fade_state = kBubbleShowingFadeIn;
  StatusBubbleState target_state = kBubbleShown;
  NSTimeInterval full_duration = kShowFadeInDurationSeconds;
  CGFloat opacity = kBubbleOpacity;

  if (!show) {
    fade_state = kBubbleHidingFadeOut;
    target_state = kBubbleHidden;
    full_duration = kHideFadeOutDurationSeconds;
    opacity = 0.0;
  }

  DCHECK(state_ == fade_state || state_ == target_state);

  if (state_ == target_state)
    return;

  if (immediate_) {
    [window_ setAlphaValue:opacity];
    SetState(target_state);
    return;
  }

  // If an incomplete transition has left the opacity somewhere between 0 and
  // kBubbleOpacity, the fade rate is kept constant by shortening the duration.
  NSTimeInterval duration =
      full_duration *
      fabs(opacity - [[window_ animator] alphaValue]) / kBubbleOpacity;

  // 0.0 will not cancel an in-progress animation.
  if (duration == 0.0)
    duration = kMinimumTimeInterval;

  // This will cancel an in-progress transition and replace it with this fade.
  [NSAnimationContext beginGrouping];
  // Don't use the GTM additon for the "Steve" slowdown because this can happen
  // async from user actions and the effects could be a surprise.
  [[NSAnimationContext currentContext] setDuration:duration];
  [[window_ animator] setAlphaValue:opacity];
  [NSAnimationContext endGrouping];
}

void StatusBubbleMac::StartTimer(int64 delay_ms) {
  DCHECK([NSThread isMainThread]);
  DCHECK(state_ == kBubbleShowingTimer || state_ == kBubbleHidingTimer);

  if (immediate_) {
    TimerFired();
    return;
  }

  // There can only be one running timer.
  CancelTimer();

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      timer_factory_.NewRunnableMethod(&StatusBubbleMac::TimerFired),
      delay_ms);
}

void StatusBubbleMac::CancelTimer() {
  DCHECK([NSThread isMainThread]);

  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();
}

void StatusBubbleMac::TimerFired() {
  DCHECK(state_ == kBubbleShowingTimer || state_ == kBubbleHidingTimer);
  DCHECK([NSThread isMainThread]);

  if (state_ == kBubbleShowingTimer) {
    SetState(kBubbleShowingFadeIn);
    Fade(true);
  } else {
    SetState(kBubbleHidingFadeOut);
    Fade(false);
  }
}

void StatusBubbleMac::StartShowing() {
  // Note that |SetState()| will |Attach()| or |Detach()| as required.

  if (state_ == kBubbleHidden) {
    // Arrange to begin fading in after a delay.
    SetState(kBubbleShowingTimer);
    StartTimer(kShowDelayMilliseconds);
  } else if (state_ == kBubbleHidingFadeOut) {
    // Cancel the fade-out in progress and replace it with a fade in.
    SetState(kBubbleShowingFadeIn);
    Fade(true);
  } else if (state_ == kBubbleHidingTimer) {
    // The bubble was already shown but was waiting to begin fading out.  It's
    // given a stay of execution.
    SetState(kBubbleShown);
    CancelTimer();
  } else if (state_ == kBubbleShowingTimer) {
    // The timer was already running but nothing was showing yet.  Reaching
    // this point means that there is a new request to show something.  Start
    // over again by resetting the timer, effectively invalidating the earlier
    // request.
    StartTimer(kShowDelayMilliseconds);
  }

  // If the state is kBubbleShown or kBubbleShowingFadeIn, leave everything
  // alone.
}

void StatusBubbleMac::StartHiding() {
  if (state_ == kBubbleShown) {
    // Arrange to begin fading out after a delay.
    SetState(kBubbleHidingTimer);
    StartTimer(kHideDelayMilliseconds);
  } else if (state_ == kBubbleShowingFadeIn) {
    // Cancel the fade-in in progress and replace it with a fade out.
    SetState(kBubbleHidingFadeOut);
    Fade(false);
  } else if (state_ == kBubbleShowingTimer) {
    // The bubble was already hidden but was waiting to begin fading in.  Too
    // bad, it won't get the opportunity now.
    SetState(kBubbleHidden);
    CancelTimer();
  }

  // If the state is kBubbleHidden, kBubbleHidingFadeOut, or
  // kBubbleHidingTimer, leave everything alone.  The timer is not reset as
  // with kBubbleShowingTimer in StartShowing() because a subsequent request
  // to hide something while one is already in flight does not invalidate the
  // earlier request.
}

void StatusBubbleMac::CancelExpandTimer() {
  DCHECK([NSThread isMainThread]);
  expand_timer_factory_.RevokeAll();
}

void StatusBubbleMac::ExpandBubble() {
  // Calculate the width available for expanded and standard bubbles.
  NSRect window_frame = CalculateWindowFrame(/*expand=*/true);
  CGFloat max_bubble_width = NSWidth(window_frame);
  CGFloat standard_bubble_width =
      NSWidth(CalculateWindowFrame(/*expand=*/false));

  // Generate the URL string that fits in the expanded bubble.
  NSFont* font = [[window_ contentView] font];
  gfx::Font font_chr(base::SysNSStringToUTF16([font fontName]),
      [font pointSize]);
  string16 expanded_url = ui::ElideUrl(url_, font_chr,
      max_bubble_width, UTF16ToWideHack(languages_));

  // Scale width from gfx::Font in view coordinates to window coordinates.
  int required_width_for_string =
      font_chr.GetStringWidth(expanded_url) +
          kTextPadding * 2 + kBubbleViewTextPositionX;
  NSSize scaled_width = NSMakeSize(required_width_for_string, 0);
  scaled_width = [[parent_ contentView] convertSize:scaled_width toView:nil];
  required_width_for_string = scaled_width.width;

  // The expanded width must be at least as wide as the standard width, but no
  // wider than the maximum width for its parent frame.
  int expanded_bubble_width =
      std::max(standard_bubble_width,
               std::min(max_bubble_width,
                        static_cast<CGFloat>(required_width_for_string)));

  SetText(expanded_url, true);
  is_expanded_ = true;
  window_frame.size.width = expanded_bubble_width;

  // In testing, don't do any animation.
  if (immediate_) {
    [window_ setFrame:window_frame display:YES];
    return;
  }

  NSRect actual_window_frame = [window_ frame];
  // Adjust status bubble origin if bubble was moved to the right.
  // TODO(alekseys): fix for RTL.
  if (NSMinX(actual_window_frame) > NSMinX(window_frame)) {
    actual_window_frame.origin.x =
        NSMaxX(actual_window_frame) - NSWidth(window_frame);
  }
  actual_window_frame.size.width = NSWidth(window_frame);

  // Do not expand if it's going to cover mouse location.
  if (NSPointInRect([NSEvent mouseLocation], actual_window_frame))
    return;

  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kExpansionDuration];
  [[window_ animator] setFrame:actual_window_frame display:YES];
  [NSAnimationContext endGrouping];
}

void StatusBubbleMac::UpdateSizeAndPosition() {
  if (!window_)
    return;

  [window_ setFrame:CalculateWindowFrame(/*expand=*/false) display:YES];
}

void StatusBubbleMac::SwitchParentWindow(NSWindow* parent) {
  DCHECK(parent);

  // If not attached, just update our member variable and position.
  if (!is_attached()) {
    parent_ = parent;
    [[window_ contentView] setThemeProvider:parent];
    UpdateSizeAndPosition();
    return;
  }

  Detach();
  parent_ = parent;
  [[window_ contentView] setThemeProvider:parent];
  Attach();
  UpdateSizeAndPosition();
}

NSRect StatusBubbleMac::CalculateWindowFrame(bool expanded_width) {
  DCHECK(parent_);

  NSRect screenRect;
  if ([delegate_ respondsToSelector:@selector(statusBubbleBaseFrame)]) {
    screenRect = [delegate_ statusBubbleBaseFrame];
    screenRect.origin = [parent_ convertBaseToScreen:screenRect.origin];
  } else {
    screenRect = [parent_ frame];
  }

  NSSize size = NSMakeSize(0, kWindowHeight);
  size = [[parent_ contentView] convertSize:size toView:nil];

  if (expanded_width) {
    size.width = screenRect.size.width;
  } else {
    size.width = kWindowWidthPercent * screenRect.size.width;
  }

  screenRect.size = size;
  return screenRect;
}
