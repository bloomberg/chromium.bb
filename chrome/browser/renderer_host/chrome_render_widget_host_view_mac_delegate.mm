// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"
#include "chrome/browser/spellchecker/spellcheck_platform.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::RenderViewHost;

@interface ChromeRenderWidgetHostViewMacDelegate () <HistorySwiperDelegate>
- (void)spellCheckEnabled:(BOOL)enabled checked:(BOOL)checked;
@end

namespace ChromeRenderWidgetHostViewMacDelegateInternal {

// Filters the message sent by the renderer to know if spellchecking is enabled
// or not for the currently focused element.
class SpellCheckObserver : public content::WebContentsObserver {
 public:
  SpellCheckObserver(
      RenderViewHost* host,
      ChromeRenderWidgetHostViewMacDelegate* view_delegate)
      : content::WebContentsObserver(
            content::WebContents::FromRenderViewHost(host)),
        view_delegate_(view_delegate) {
  }

  ~SpellCheckObserver() override {}

 private:
  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SpellCheckObserver, message)
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

- (id)initWithRenderWidgetHost:(content::RenderWidgetHost*)renderWidgetHost {
  self = [super init];
  if (self) {
    renderWidgetHost_ = renderWidgetHost;
    RenderViewHost* rvh = RenderViewHost::From(renderWidgetHost_);
    if (rvh) {
      spellingObserver_.reset(
          new ChromeRenderWidgetHostViewMacDelegateInternal::SpellCheckObserver(
              rvh, self));
    }

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

- (BOOL)canRubberbandLeft:(NSView*)view {
  return [historySwiper_ canRubberbandLeft:view];
}

- (BOOL)canRubberbandRight:(NSView*)view {
  return [historySwiper_ canRubberbandRight:view];
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
      spellcheckChecked_ =
          profile->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck);
      NSCellStateValue checkedState =
          spellcheckChecked_ ? NSOnState : NSOffState;
      [(id)item setState:checkedState];
    }
    *valid = spellcheckEnabled_;
    return YES;
  }

  return NO;
}

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  [historySwiper_ rendererHandledWheelEvent:event consumed:consumed];
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
    webContents->Replace(base::SysNSStringToUTF16(newWord));
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
  pref->SetBoolean(prefs::kEnableContinuousSpellcheck,
                   !pref->GetBoolean(prefs::kEnableContinuousSpellcheck));
}

- (void)spellCheckEnabled:(BOOL)enabled checked:(BOOL)checked {
  spellcheckEnabled_ = enabled;
  spellcheckChecked_ = checked;
}

// END Spellchecking methods

@end
