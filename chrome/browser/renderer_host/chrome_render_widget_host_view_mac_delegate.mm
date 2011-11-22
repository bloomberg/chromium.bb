// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"

#include <cmath>

#include "base/sys_string_conversions.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/spellchecker/spellchecker_platform_engine.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/history_overlay_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/browser/mac/closure_blocks_leopard_compat.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_observer.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"

// Declare things that are part of the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
enum {
  NSEventPhaseNone        = 0, // event not associated with a phase.
  NSEventPhaseBegan       = 0x1 << 0,
  NSEventPhaseStationary  = 0x1 << 1,
  NSEventPhaseChanged     = 0x1 << 2,
  NSEventPhaseEnded       = 0x1 << 3,
  NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

enum {
  NSEventSwipeTrackingLockDirection = 0x1 << 0,
  NSEventSwipeTrackingClampGestureAmount = 0x1 << 1
};
typedef NSUInteger NSEventSwipeTrackingOptions;

@interface NSEvent (LionAPI)
+ (BOOL)isSwipeTrackingFromScrollEventsEnabled;

- (NSEventPhase)phase;
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options
          dampenAmountThresholdMin:(CGFloat)minDampenThreshold
                               max:(CGFloat)maxDampenThreshold
                      usingHandler:(void (^)(CGFloat gestureAmount,
                                             NSEventPhase phase,
                                             BOOL isComplete,
                                             BOOL *stop))trackingHandler;
@end
#endif  // 10.7

@interface ChromeRenderWidgetHostViewMacDelegate ()
- (BOOL)maybeHandleHistorySwiping:(NSEvent*)theEvent;
- (void)spellCheckEnabled:(BOOL)enabled checked:(BOOL)checked;
@end

namespace ChromeRenderWidgetHostViewMacDelegateInternal {

// Filters the message sent to RenderViewHost to know if spellchecking is
// enabled or not for the currently focused element.
class SpellCheckRenderViewObserver : public RenderViewHostObserver {
 public:
  SpellCheckRenderViewObserver(
      RenderViewHost* host,
      ChromeRenderWidgetHostViewMacDelegate* view_delegate)
      : RenderViewHostObserver(host),
        view_delegate_(view_delegate) {
  }

  virtual ~SpellCheckRenderViewObserver() {
  }

 private:
  // RenderViewHostObserver implementation.
  virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE {
    // The parent implementation destroys the observer, scoping the lifetime of
    // the observer to the RenderViewHost. Since this class is acting as a
    // bridge to the view for the delegate below, and is owned by that delegate,
    // undo the scoping by not calling through to the parent implementation.
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SpellCheckRenderViewObserver, message)
      IPC_MESSAGE_HANDLER(SpellCheckHostMsg_ToggleSpellCheck,
                          OnToggleSpellCheck)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnToggleSpellCheck(bool enabled, bool checked) {
    [view_delegate_ spellCheckEnabled:enabled checked:checked];
  }

  ChromeRenderWidgetHostViewMacDelegate* view_delegate_;
};

}  // namespace ChromeRenderWidgetHostViewMacDelegateInternal

@implementation ChromeRenderWidgetHostViewMacDelegate

- (id)initWithRenderWidgetHost:(RenderWidgetHost*)render_widget_host {
  self = [super init];
  if (self) {
    render_widget_host_ = render_widget_host;
    NSView* native_view = render_widget_host_->view()->GetNativeView();
    view_id_util::SetID(native_view, VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);

    if (render_widget_host_->IsRenderView()) {
      spelling_observer_.reset(
          new ChromeRenderWidgetHostViewMacDelegateInternal::
              SpellCheckRenderViewObserver(
                  static_cast<RenderViewHost*>(render_widget_host_), self));
    }
  }
  return self;
}

- (void)viewGone:(NSView*)view {
  view_id_util::UnsetID(view);
  [self autorelease];
}

- (BOOL)handleEvent:(NSEvent*)event {
  if ([event type] == NSScrollWheel)
    return [self maybeHandleHistorySwiping:event];

  return NO;
}

- (void)gotUnhandledWheelEvent {
  gotUnhandledWheelEvent_ = true;
}

- (void)scrollOffsetPinnedToLeft:(BOOL)left toRight:(BOOL)right {
  isPinnedLeft_ = left;
  isPinnedRight_ = right;
}

- (void)setHasHorizontalScrollbar:(BOOL)has_horizontal_scrollbar {
  hasHorizontalScrollbar_ = has_horizontal_scrollbar;
}

// Checks if |theEvent| should trigger history swiping, and if so, does
// history swiping. Returns YES if the event was consumed or NO if it should
// be passed on to the renderer.
- (BOOL)maybeHandleHistorySwiping:(NSEvent*)theEvent {
  BOOL canUseLionApis = [theEvent respondsToSelector:@selector(phase)];
  if (!canUseLionApis)
    return NO;

  // Scroll events always go to the web first, and can only trigger history
  // swiping if they come back unhandled.
  if ([theEvent phase] == NSEventPhaseBegan) {
    totalScrollDelta_ = NSZeroSize;
    gotUnhandledWheelEvent_ = false;
  }

  if (!render_widget_host_ || !render_widget_host_->IsRenderView())
    return NO;
  bool isDevtoolsRwhv = DevToolsWindow::IsDevToolsWindow(
      static_cast<RenderViewHost*>(render_widget_host_));
  if (isDevtoolsRwhv)
    return NO;

  if (gotUnhandledWheelEvent_ &&
      [NSEvent isSwipeTrackingFromScrollEventsEnabled] &&
      [theEvent phase] == NSEventPhaseChanged) {
    totalScrollDelta_.width += [theEvent scrollingDeltaX];
    totalScrollDelta_.height += [theEvent scrollingDeltaY];

    bool isHorizontalGesture =
      std::abs(totalScrollDelta_.width) > std::abs(totalScrollDelta_.height);

    bool isRightScroll = [theEvent scrollingDeltaX] < 0;
    bool goForward = isRightScroll;
    bool canGoBack = false, canGoForward = false;
    if (Browser* browser = BrowserList::GetLastActive()) {
      canGoBack = browser->CanGoBack();
      canGoForward = browser->CanGoForward();
    }

    // If "forward" is inactive and the user rubber-bands to the right,
    // "isPinnedLeft" will be false.  When the user then rubber-bands to the
    // left in the same gesture, that should trigger history immediately if
    // there's no scrollbar, hence the check for hasHorizontalScrollbar_.
    bool shouldGoBack = isPinnedLeft_ || !hasHorizontalScrollbar_;
    bool shouldGoForward = isPinnedRight_ || !hasHorizontalScrollbar_;
    if (isHorizontalGesture &&
        // For normal pages, canGoBack/canGoForward are checked in the renderer
        // (ChromeClientImpl::shouldRubberBand()), when it decides if it should
        // rubberband or send back an event unhandled. The check here is
        // required for pages with an onmousewheel handler that doesn't call
        // preventDefault().
        ((shouldGoBack && canGoBack && !isRightScroll) ||
         (shouldGoForward && canGoForward && isRightScroll))) {

      // Released by the tracking handler once the gesture is complete.
      HistoryOverlayController* historyOverlay =
          [[HistoryOverlayController alloc]
            initForMode:goForward ? kHistoryOverlayModeForward :
                                    kHistoryOverlayModeBack];

      // The way this API works: gestureAmount is between -1 and 1 (float).  If
      // the user does the gesture for more than about 25% (i.e. < -0.25 or >
      // 0.25) and then lets go, it is accepted, we get a NSEventPhaseEnded,
      // and after that the block is called with amounts animating towards 1
      // (or -1, depending on the direction).  If the user lets go below that
      // threshold, we get NSEventPhaseCancelled, and the amount animates
      // toward 0.  When gestureAmount has reaches its final value, i.e. the
      // track animation is done, the handler is called with |isComplete| set
      // to |YES|.
      [theEvent trackSwipeEventWithOptions:0
                  dampenAmountThresholdMin:-1
                                       max:1
                              usingHandler:^(CGFloat gestureAmount,
                                             NSEventPhase phase,
                                             BOOL isComplete,
                                             BOOL *stop) {
          if (phase == NSEventPhaseBegan) {
            NSWindow* window =
                [render_widget_host_->view()->GetNativeView() window];
            [historyOverlay showPanelForWindow:window];
            return;
          }

          // |gestureAmount| obeys -[NSEvent isDirectionInvertedFromDevice]
          // automatically.
          Browser* browser = BrowserList::GetLastActive();
          if (phase == NSEventPhaseEnded && browser) {
            if (goForward)
              browser->GoForward(CURRENT_TAB);
            else
              browser->GoBack(CURRENT_TAB);
          }

          [historyOverlay setProgress:gestureAmount];
          if (isComplete) {
            [historyOverlay dismiss];
            [historyOverlay release];
          }
        }];
      return YES;
    }
  }
  return NO;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid {
  SEL action = [item action];

  // For now, this action is always enabled for render view;
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(checkSpelling:)) {
    *valid = render_widget_host_->IsRenderView();
    return YES;
  }

  if (action == @selector(toggleContinuousSpellChecking:)) {
    if ([(id)item respondsToSelector:@selector(setState:)]) {
      NSCellStateValue checked_state =
          spellcheck_checked_ ? NSOnState : NSOffState;
      [(id)item setState:checked_state];
    }
    *valid = spellcheck_enabled_;
    return YES;
  }

  return NO;
}

// Spellchecking methods
// The next five methods are implemented here since this class is the first
// responder for anything in the browser.

// This message is sent whenever the user specifies that a word should be
// changed from the spellChecker.
- (void)changeSpelling:(id)sender {
  // Grab the currently selected word from the spell panel, as this is the word
  // that we want to replace the selected word in the text with.
  NSString* newWord = [[sender selectedCell] stringValue];
  if (newWord != nil) {
    render_widget_host_->Replace(base::SysNSStringToUTF16(newWord));
  }
}

// This message is sent by NSSpellChecker whenever the next word should be
// advanced to, either after a correction or clicking the "Find Next" button.
// This isn't documented anywhere useful, like in NSSpellProtocol.h with the
// other spelling panel methods. This is probably because Apple assumes that the
// the spelling panel will be used with an NSText, which will automatically
// catch this and advance to the next word for you. Thanks Apple.
// This is also called from the Edit -> Spelling -> Check Spelling menu item.
- (void)checkSpelling:(id)sender {
  render_widget_host_->Send(new SpellCheckMsg_AdvanceToNextMisspelling(
      render_widget_host_->routing_id()));
}

// This message is sent by the spelling panel whenever a word is ignored.
- (void)ignoreSpelling:(id)sender {
  // Ideally, we would ask the current RenderView for its tag, but that would
  // mean making a blocking IPC call from the browser. Instead,
  // SpellCheckerPlatform::CheckSpelling remembers the last tag and
  // SpellCheckerPlatform::IgnoreWord assumes that is the correct tag.
  NSString* wordToIgnore = [sender stringValue];
  if (wordToIgnore != nil)
    SpellCheckerPlatform::IgnoreWord(base::SysNSStringToUTF16(wordToIgnore));
}

- (void)showGuessPanel:(id)sender {
  render_widget_host_->Send(new SpellCheckMsg_ToggleSpellPanel(
      render_widget_host_->routing_id(),
      SpellCheckerPlatform::SpellingPanelVisible()));
}

- (void)toggleContinuousSpellChecking:(id)sender {
  render_widget_host_->Send(
      new SpellCheckMsg_ToggleSpellCheck(render_widget_host_->routing_id()));
}

- (void)spellCheckEnabled:(BOOL)enabled checked:(BOOL)checked {
  spellcheck_enabled_ = enabled;
  spellcheck_checked_ = checked;
}

// END Spellchecking methods

@end
