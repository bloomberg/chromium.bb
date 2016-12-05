// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;

@interface ChromeRenderWidgetHostViewMacDelegate () <HistorySwiperDelegate>
@end

@implementation ChromeRenderWidgetHostViewMacDelegate

- (id)initWithRenderWidgetHost:(content::RenderWidgetHost*)renderWidgetHost {
  self = [super init];
  if (self) {
    renderWidgetHost_ = renderWidgetHost;
    historySwiper_.reset([[HistorySwiper alloc] initWithDelegate:self]);
  }
  return self;
}

- (void)dealloc {
  [historySwiper_ setDelegate:nil];
  [super dealloc];
}

// Handle an event. All incoming key and mouse events flow through this
// delegate method if implemented. Return YES if the event is fully handled, or
// NO if normal processing should take place.
- (BOOL)handleEvent:(NSEvent*)event {
  return [historySwiper_ handleEvent:event];
}

// NSWindow events.

- (void)beginGestureWithEvent:(NSEvent*)event {
  [historySwiper_ beginGestureWithEvent:event];
}

- (void)endGestureWithEvent:(NSEvent*)event {
  [historySwiper_ endGestureWithEvent:event];
}

// This is a low level API which provides touches associated with an event.
// It is used in conjunction with gestures to determine finger placement
// on the trackpad.
- (void)touchesMovedWithEvent:(NSEvent*)event {
  [historySwiper_ touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [historySwiper_ touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [historySwiper_ touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [historySwiper_ touchesEndedWithEvent:event];
}

// HistorySwiperDelegate methods

- (BOOL)shouldAllowHistorySwiping {
  if (!renderWidgetHost_)
    return NO;
  RenderViewHost* renderViewHost = RenderViewHost::From(renderWidgetHost_);
  if (!renderViewHost)
    return NO;
  content::WebContents* webContents =
      content::WebContents::FromRenderViewHost(renderViewHost);
  if (webContents && DevToolsWindow::IsDevToolsWindow(webContents)) {
    return NO;
  }

  return YES;
}

- (NSView*)viewThatWantsHistoryOverlay {
  return renderWidgetHost_->GetView()->GetNativeView();
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid {
  SEL action = [item action];

  // For now, this action is always enabled for render view;
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(checkSpelling:)) {
    *valid = RenderViewHost::From(renderWidgetHost_) != nullptr;
    return YES;
  }

  // TODO(groby): Clarify who sends this and if toggleContinuousSpellChecking:
  // is still necessary.
  if (action == @selector(toggleContinuousSpellChecking:)) {
    if ([(id)item respondsToSelector:@selector(setState:)]) {
      content::RenderProcessHost* host = renderWidgetHost_->GetProcess();
      Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
      DCHECK(profile);
      NSCellStateValue checkedState =
          profile->GetPrefs()->GetBoolean(
              spellcheck::prefs::kEnableSpellcheck)
              ? NSOnState
              : NSOffState;
      [(id)item setState:checkedState];
    }
    *valid = YES;
    return YES;
  }

  return NO;
}

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  [historySwiper_ rendererHandledWheelEvent:event consumed:consumed];
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  [historySwiper_ rendererHandledGestureScrollEvent:event consumed:consumed];
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
    content::WebContents* webContents =
        content::WebContents::FromRenderViewHost(
            RenderViewHost::From(renderWidgetHost_));
    webContents->ReplaceMisspelling(base::SysNSStringToUTF16(newWord));
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
  renderWidgetHost_->Send(new SpellCheckMsg_AdvanceToNextMisspelling(
      renderWidgetHost_->GetRoutingID()));
}

// This message is sent by the spelling panel whenever a word is ignored.
- (void)ignoreSpelling:(id)sender {
  // Ideally, we would ask the current RenderView for its tag, but that would
  // mean making a blocking IPC call from the browser. Instead,
  // spellcheck_platform::CheckSpelling remembers the last tag and
  // spellcheck_platform::IgnoreWord assumes that is the correct tag.
  NSString* wordToIgnore = [sender stringValue];
  if (wordToIgnore != nil)
    spellcheck_platform::IgnoreWord(base::SysNSStringToUTF16(wordToIgnore));
}

- (void)showGuessPanel:(id)sender {
  renderWidgetHost_->Send(new SpellCheckMsg_ToggleSpellPanel(
      renderWidgetHost_->GetRoutingID(),
      spellcheck_platform::SpellingPanelVisible()));
}

- (void)toggleContinuousSpellChecking:(id)sender {
  content::RenderProcessHost* host = renderWidgetHost_->GetProcess();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  DCHECK(profile);
  PrefService* pref = profile->GetPrefs();
  pref->SetBoolean(
      spellcheck::prefs::kEnableSpellcheck,
      !pref->GetBoolean(spellcheck::prefs::kEnableSpellcheck));
}

// END Spellchecking methods

@end
