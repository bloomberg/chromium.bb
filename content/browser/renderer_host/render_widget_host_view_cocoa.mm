// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_view_cocoa.h"

#include <limits>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#import "content/browser/cocoa/system_hotkey_helper_mac.h"
#import "content/browser/cocoa/system_hotkey_map.h"
#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"
#import "content/browser/renderer_host/text_input_client_mac.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/appkit_utils.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/cocoa/text_services_context_menu.h"
#include "ui/events/event_utils.h"

using content::BrowserAccessibility;
using content::BrowserAccessibilityManager;
using content::EditCommand;
using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::RenderWidgetHostImpl;
using content::RenderWidgetHostNSViewClient;
using content::RenderWidgetHostView;
using content::RenderWidgetHostViewMac;
using content::RenderWidgetHostViewMacEditCommandHelper;
using content::TextInputClientMac;
using content::WebContents;
using content::WebGestureEventBuilder;
using content::WebMouseEventBuilder;
using content::WebMouseWheelEventBuilder;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebGestureEvent;

namespace {

// Whether a keyboard event has been reserved by OSX.
BOOL EventIsReservedBySystem(NSEvent* event) {
  content::SystemHotkeyHelperMac* helper =
      content::SystemHotkeyHelperMac::GetInstance();
  return helper->map()->IsEventReserved(event);
}

RenderWidgetHostView* GetRenderWidgetHostViewToUse(
    RenderWidgetHostViewMac* render_widget_host_view) {
  WebContents* web_contents = render_widget_host_view->GetWebContents();
  if (!web_contents)
    return render_widget_host_view;
  content::BrowserPluginGuestManager* guest_manager =
      web_contents->GetBrowserContext()->GetGuestManager();
  if (!guest_manager)
    return render_widget_host_view;
  content::WebContents* guest = guest_manager->GetFullPageGuest(web_contents);
  if (!guest)
    return render_widget_host_view;
  return guest->GetRenderWidgetHostView();
}

// TODO(suzhe): Upstream this function.
blink::WebColor WebColorFromNSColor(NSColor* color) {
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];

  return std::max(0, std::min(static_cast<int>(lroundf(255.0f * a)), 255))
             << 24 |
         std::max(0, std::min(static_cast<int>(lroundf(255.0f * r)), 255))
             << 16 |
         std::max(0, std::min(static_cast<int>(lroundf(255.0f * g)), 255))
             << 8 |
         std::max(0, std::min(static_cast<int>(lroundf(255.0f * b)), 255));
}

// Extract underline information from an attributed string. Mostly copied from
// third_party/WebKit/Source/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(NSAttributedString* string,
                       std::vector<ui::ImeTextSpan>* ime_text_spans) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber* style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
      blink::WebColor color = SK_ColorBLACK;
      if (NSColor* colorAttr =
              [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = WebColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      ui::ImeTextSpan::Thickness thickness =
          [style intValue] > 1 ? ui::ImeTextSpan::Thickness::kThick
                               : ui::ImeTextSpan::Thickness::kThin;
      ui::ImeTextSpan ui_ime_text_span =
          ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, range.location,
                          NSMaxRange(range), thickness, SK_ColorTRANSPARENT);
      ui_ime_text_span.underline_color = color;
      ime_text_spans->push_back(ui_ime_text_span);
    }
    i = range.location + range.length;
  }
}

}  // namespace

// These are not documented, so use only after checking -respondsToSelector:.
@interface NSApplication (UndocumentedSpeechMethods)
- (void)speakString:(NSString*)string;
- (void)stopSpeaking:(id)sender;
- (BOOL)isSpeaking;
@end

// RenderWidgetHostViewCocoa ---------------------------------------------------

// Private methods:
@interface RenderWidgetHostViewCocoa ()
- (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event;
- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed;
- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;
- (void)windowChangedGlobalFrame:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (void)windowDidResignKey:(NSNotification*)notification;
- (void)updateScreenProperties;
- (void)showLookUpDictionaryOverlayInternal:(NSAttributedString*)string
                              baselinePoint:(NSPoint)baselinePoint
                                 targetView:(NSView*)view;
@end

@implementation RenderWidgetHostViewCocoa
@synthesize selectedRange = selectedRange_;
@synthesize suppressNextEscapeKeyUp = suppressNextEscapeKeyUp_;
@synthesize markedRange = markedRange_;

- (id)initWithClient:(std::unique_ptr<RenderWidgetHostNSViewClient>)client {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    self.acceptsTouchEvents = YES;
    editCommand_helper_.reset(new RenderWidgetHostViewMacEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    client_ = std::move(client);
    renderWidgetHostView_ = client_->GetRenderWidgetHostViewMac();
    canBeKeyView_ = YES;
    pinchHasReachedZoomThreshold_ = false;
    isStylusEnteringProximity_ = false;
  }
  return self;
}

- (void)dealloc {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(viewGone:)])
    [responderDelegate_ viewGone:self];
  responderDelegate_.reset();

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // Update and cache the new input context. Otherwise,
  // [NSTextInputContext currentInputContext] might still hold on to this
  // view's NSTextInputContext even after it's deallocated.
  // See http://crbug.com/684388.
  [[self window] makeFirstResponder:nil];
  [NSApp updateWindows];

  // Debug key to check if the current input context still holds onto the view.
  NSTextInputContext* currentContext = [NSTextInputContext currentInputContext];
  auto* crashKey = base::debug::AllocateCrashKeyString(
      "text-input-context-client", base::debug::CrashKeySize::Size32);
  base::debug::ScopedCrashKeyString textInputContextCrashKey(
      crashKey, currentContext && [currentContext client] == self
                    ? "text input still held on"
                    : "text input no longer held on");

  [super dealloc];
}

- (void)setResponderDelegate:
    (NSObject<RenderWidgetHostViewMacDelegate>*)delegate {
  DCHECK(!responderDelegate_);
  responderDelegate_.reset([delegate retain]);
}

- (void)resetCursorRects {
  if (currentCursor_) {
    [self addCursorRect:[self visibleRect] cursor:currentCursor_];
    [currentCursor_ setOnMouseEntered:YES];
  }
}

- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed {
  [responderDelegate_ rendererHandledWheelEvent:event consumed:consumed];
}

- (void)processedGestureScrollEvent:(const blink::WebGestureEvent&)event
                           consumed:(BOOL)consumed {
  [responderDelegate_ rendererHandledGestureScrollEvent:event
                                               consumed:consumed];
}

- (void)processedOverscroll:(const ui::DidOverscrollParams&)params {
  [responderDelegate_ rendererHandledOverscrollEvent:params];
}

- (BOOL)respondsToSelector:(SEL)selector {
  // Trickiness: this doesn't mean "does this object's superclass respond to
  // this selector" but rather "does the -respondsToSelector impl from the
  // superclass say that this class responds to the selector".
  if ([super respondsToSelector:selector])
    return YES;

  if (responderDelegate_)
    return [responderDelegate_ respondsToSelector:selector];

  return NO;
}

- (id)forwardingTargetForSelector:(SEL)selector {
  if ([responderDelegate_ respondsToSelector:selector])
    return responderDelegate_.get();

  return [super forwardingTargetForSelector:selector];
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (BOOL)acceptsMouseEventsWhenInactive {
  // Some types of windows (balloons, always-on-top panels) want to accept mouse
  // clicks w/o the first click being treated as 'activation'. Same applies to
  // mouse move events.
  return [[self window] level] > NSNormalWindowLevel;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return [self acceptsMouseEventsWhenInactive];
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (BOOL)shouldIgnoreMouseEvent:(NSEvent*)theEvent {
  NSWindow* window = [self window];
  // If this is a background window, don't handle mouse movement events. This
  // is the expected behavior on the Mac as evidenced by other applications.
  if ([theEvent type] == NSMouseMoved &&
      ![self acceptsMouseEventsWhenInactive] && ![window isKeyWindow]) {
    return YES;
  }

  // Use hitTest to check whether the mouse is over a nonWebContentView - in
  // which case the mouse event should not be handled by the render host.
  const SEL nonWebContentViewSelector = @selector(nonWebContentView);
  NSView* contentView = [window contentView];
  NSView* view = [contentView hitTest:[theEvent locationInWindow]];
  // Traverse the superview hierarchy as the hitTest will return the frontmost
  // view, such as an NSTextView, while nonWebContentView may be specified by
  // its parent view.
  while (view) {
    if ([view respondsToSelector:nonWebContentViewSelector] &&
        [view performSelector:nonWebContentViewSelector]) {
      // The cursor is over a nonWebContentView - ignore this mouse event.
      return YES;
    }
    if ([view isKindOfClass:[self class]] && ![view isEqual:self] &&
        !hasOpenMouseDown_) {
      // The cursor is over an overlapping render widget. This check is done by
      // both views so the one that's returned by -hitTest: will end up
      // processing the event.
      // Note that while dragging, we only get events for the render view where
      // drag started, even if mouse is  actually over another view or outside
      // the window. Cocoa does this for us. We should handle these events and
      // not ignore (since there is no other render view to handle them). Thus
      // the |!hasOpenMouseDown_| check above.
      return YES;
    }
    view = [view superview];
  }
  return NO;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::mouseEvent");
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return;
  }

  // Set the pointer type when we are receiving a NSMouseEntered event and the
  // following NSMouseExited event should have the same pointer type.
  // For NSMouseExited and NSMouseEntered events, they do not have a subtype.
  // We decide their pointer types by checking if we recevied a
  // NSTabletProximity event.
  NSEventType type = [theEvent type];
  if (type == NSMouseEntered || type == NSMouseExited) {
    pointerType_ = isStylusEnteringProximity_
                       ? pointerType_
                       : blink::WebPointerProperties::PointerType::kMouse;
  } else {
    NSEventSubtype subtype = [theEvent subtype];
    // For other mouse events and touchpad events, the pointer type is mouse.
    if (subtype != NSTabletPointEventSubtype &&
        subtype != NSTabletProximityEventSubtype) {
      pointerType_ = blink::WebPointerProperties::PointerType::kMouse;
    }
  }

  if ([self shouldIgnoreMouseEvent:theEvent]) {
    // If this is the first such event, send a mouse exit to the host view.
    if (!mouseEventWasIgnored_ && renderWidgetHostView_->host()) {
      WebMouseEvent exitEvent =
          WebMouseEventBuilder::Build(theEvent, self, pointerType_);
      exitEvent.SetType(WebInputEvent::kMouseLeave);
      exitEvent.button = WebMouseEvent::Button::kNoButton;
      renderWidgetHostView_->ForwardMouseEvent(exitEvent);
    }
    mouseEventWasIgnored_ = YES;
    return;
  }

  if (mouseEventWasIgnored_) {
    // If this is the first mouse event after a previous event that was ignored
    // due to the hitTest, send a mouse enter event to the host view.
    if (renderWidgetHostView_->host()) {
      WebMouseEvent enterEvent =
          WebMouseEventBuilder::Build(theEvent, self, pointerType_);
      enterEvent.SetType(WebInputEvent::kMouseMove);
      enterEvent.button = WebMouseEvent::Button::kNoButton;
      ui::LatencyInfo latency_info(ui::SourceEventType::OTHER);
      latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
      if (renderWidgetHostView_->ShouldRouteEvent(enterEvent)) {
        renderWidgetHostView_->host()
            ->delegate()
            ->GetInputEventRouter()
            ->RouteMouseEvent(renderWidgetHostView_, &enterEvent, latency_info);
      } else {
        renderWidgetHostView_->ProcessMouseEvent(enterEvent, latency_info);
      }
    }
  }
  mouseEventWasIgnored_ = NO;

  // Don't cancel child popups; killing them on a mouse click would prevent the
  // user from positioning the insertion point in the text field spawning the
  // popup. A click outside the text field would cause the text field to drop
  // the focus, and then EditorClientImpl::textFieldDidEndEditing() would cancel
  // the popup anyway, so we're OK.
  if (type == NSLeftMouseDown)
    hasOpenMouseDown_ = YES;
  else if (type == NSLeftMouseUp)
    hasOpenMouseDown_ = NO;

  // TODO(suzhe): We should send mouse events to the input method first if it
  // wants to handle them. But it won't work without implementing method
  // - (NSUInteger)characterIndexForPoint:.
  // See: http://code.google.com/p/chromium/issues/detail?id=47141
  // Instead of sending mouse events to the input method first, we now just
  // simply confirm all ongoing composition here.
  if (type == NSLeftMouseDown || type == NSRightMouseDown ||
      type == NSOtherMouseDown) {
    [self finishComposingText];
  }

  if (type == NSMouseMoved)
    cursorHidden_ = NO;

  WebMouseEvent event =
      WebMouseEventBuilder::Build(theEvent, self, pointerType_);
  ui::LatencyInfo latency_info(ui::SourceEventType::OTHER);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
  if (renderWidgetHostView_->ShouldRouteEvent(event)) {
    renderWidgetHostView_->host()
        ->delegate()
        ->GetInputEventRouter()
        ->RouteMouseEvent(renderWidgetHostView_, &event, latency_info);
  } else {
    renderWidgetHostView_->ProcessMouseEvent(event, latency_info);
  }
}

- (void)tabletEvent:(NSEvent*)theEvent {
  if ([theEvent type] == NSTabletProximity) {
    isStylusEnteringProximity_ = [theEvent isEnteringProximity];
    NSPointingDeviceType deviceType = [theEvent pointingDeviceType];
    // For all tablet events, the pointer type will be pen or eraser.
    pointerType_ = deviceType == NSEraserPointingDevice
                       ? blink::WebPointerProperties::PointerType::kEraser
                       : blink::WebPointerProperties::PointerType::kPen;
  }
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  // |performKeyEquivalent:| is sent to all views of a window, not only down the
  // responder chain (cf. "Handling Key Equivalents" in
  // http://developer.apple.com/mac/library/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html
  // ). A |performKeyEquivalent:| may also bubble up from a dialog child window
  // to perform browser commands such as switching tabs. We only want to handle
  // key equivalents if we're first responder in the keyWindow.
  if (![[self window] isKeyWindow] || [[self window] firstResponder] != self)
    return NO;

  // If the event is reserved by the system, then do not pass it to web content.
  if (EventIsReservedBySystem(theEvent))
    return NO;

  // If we return |NO| from this function, cocoa will send the key event to
  // the menu and only if the menu does not process the event to |keyDown:|. We
  // want to send the event to a renderer _before_ sending it to the menu, so
  // we need to return |YES| for all events that might be swallowed by the menu.
  // We do not return |YES| for every keypress because we don't get |keyDown:|
  // events for keys that we handle this way.
  NSUInteger modifierFlags = [theEvent modifierFlags];
  if ((modifierFlags & NSCommandKeyMask) == 0) {
    // Make sure the menu does not contain key equivalents that don't
    // contain cmd.
    DCHECK(![[NSApp mainMenu] performKeyEquivalent:theEvent]);
    return NO;
  }

  // Command key combinations are sent via performKeyEquivalent rather than
  // keyDown:. We just forward this on and if WebCore doesn't want to handle
  // it, we let the WebContentsView figure out how to reinject it.
  [self keyEvent:theEvent wasKeyEquivalent:YES];
  return YES;
}

- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  // This is a SPI that AppKit apparently calls after |performKeyEquivalent:|
  // returned NO. If this function returns |YES|, Cocoa sends the event to
  // |keyDown:| instead of doing other things with it. Ctrl-tab will be sent
  // to us instead of doing key view loop control, ctrl-left/right get handled
  // correctly, etc.
  // (However, there are still some keys that Cocoa swallows, e.g. the key
  // equivalent that Cocoa uses for toggling the input language. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return kEventHandled;
  }

  [self keyEvent:theEvent wasKeyEquivalent:NO];
  return kEventHandled;
}

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::keyEvent");

  // If the user changes the system hotkey mapping after Chrome has been
  // launched, then it is possible that a formerly reserved system hotkey is no
  // longer reserved. The hotkey would have skipped the renderer, but would
  // also have not been handled by the system. If this is the case, immediately
  // return.
  // TODO(erikchen): SystemHotkeyHelperMac should use the File System Events
  // api to monitor changes to system hotkeys. This logic will have to be
  // updated.
  // http://crbug.com/383558.
  if (EventIsReservedBySystem(theEvent))
    return;

  DCHECK([theEvent type] != NSKeyDown ||
         !equiv == !([theEvent modifierFlags] & NSCommandKeyMask));

  if ([theEvent type] == NSFlagsChanged) {
    // Ignore NSFlagsChanged events from the NumLock and Fn keys as
    // Safari does in -[WebHTMLView flagsChanged:] (of "WebHTMLView.mm").
    // Also ignore unsupported |keyCode| (255) generated by Convert, NonConvert
    // and KanaMode from JIS PC keyboard.
    int keyCode = [theEvent keyCode];
    if (!keyCode || keyCode == 10 || keyCode == 63 || keyCode == 255)
      return;
  }

  // Don't cancel child popups; the key events are probably what's triggering
  // the popup in the first place.

  RenderWidgetHostImpl* widgetHost = renderWidgetHostView_->host();
  DCHECK(widgetHost);

  NativeWebKeyboardEvent event(theEvent);
  ui::LatencyInfo latency_info;
  if (event.GetType() == blink::WebInputEvent::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::kChar) {
    latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  }

  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);

  // Force fullscreen windows to close on Escape so they won't keep the keyboard
  // grabbed or be stuck onscreen if the renderer is hanging.
  if (event.GetType() == NativeWebKeyboardEvent::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE &&
      renderWidgetHostView_->pepper_fullscreen_window()) {
    RenderWidgetHostViewMac* parent =
        renderWidgetHostView_->fullscreen_parent_host_view();
    if (parent)
      parent->cocoa_view()->suppressNextEscapeKeyUp_ = YES;
    widgetHost->ShutdownAndDestroyWidget(true);
    return;
  }

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (widgetHost->delegate())
    widgetHost = widgetHost->delegate()->GetFocusedRenderWidgetHost(widgetHost);
  if (!widgetHost)
    return;

  // Suppress the escape key up event if necessary.
  if (event.windows_key_code == ui::VKEY_ESCAPE && suppressNextEscapeKeyUp_) {
    if (event.GetType() == NativeWebKeyboardEvent::kKeyUp)
      suppressNextEscapeKeyUp_ = NO;
    return;
  }

  // Do not forward key up events unless preceded by a matching key down,
  // otherwise we might get an event from releasing the return key in the
  // omnibox (http://crbug.com/338736).
  if ([theEvent type] == NSKeyUp) {
    auto numErased = keyDownCodes_.erase([theEvent keyCode]);
    if (numErased < 1)
      return;
  }

  // We only handle key down events and just simply forward other events.
  if ([theEvent type] != NSKeyDown) {
    widgetHost->ForwardKeyboardEventWithLatencyInfo(event, latency_info);

    // Possibly autohide the cursor.
    if ([self shouldAutohideCursorForEvent:theEvent]) {
      [NSCursor setHiddenUntilMouseMoves:YES];
      cursorHidden_ = YES;
    }

    return;
  }

  keyDownCodes_.insert([theEvent keyCode]);

  base::scoped_nsobject<RenderWidgetHostViewCocoa> keepSelfAlive([self retain]);

  // Records the current marked text state, so that we can know if the marked
  // text was deleted or not after handling the key down event.
  BOOL oldHasMarkedText = hasMarkedText_;

  // This method should not be called recursively.
  DCHECK(!handlingKeyDown_);

  // Tells insertText: and doCommandBySelector: that we are handling a key
  // down event.
  handlingKeyDown_ = YES;

  // These variables might be set when handling the keyboard event.
  // Clear them here so that we can know whether they have changed afterwards.
  textToBeInserted_.clear();
  markedText_.clear();
  markedTextSelectedRange_ = NSMakeRange(NSNotFound, 0);
  ime_text_spans_.clear();
  setMarkedTextReplacementRange_ = gfx::Range::InvalidRange();
  unmarkTextCalled_ = NO;
  hasEditCommands_ = NO;
  editCommands_.clear();

  // Sends key down events to input method first, then we can decide what should
  // be done according to input method's feedback.
  [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];

  handlingKeyDown_ = NO;

  // Indicates if we should send the key event and corresponding editor commands
  // after processing the input method result.
  BOOL delayEventUntilAfterImeCompostion = NO;

  // To emulate Windows, over-write |event.windowsKeyCode| to VK_PROCESSKEY
  // while an input method is composing or inserting a text.
  // Gmail checks this code in its onkeydown handler to stop auto-completing
  // e-mail addresses while composing a CJK text.
  // If the text to be inserted has only one character, then we don't need this
  // trick, because we'll send the text as a key press event instead.
  if (hasMarkedText_ || oldHasMarkedText || textToBeInserted_.length() > 1) {
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.windows_key_code = 0xE5;  // VKEY_PROCESSKEY
    fakeEvent.skip_in_browser = true;
    widgetHost->ForwardKeyboardEventWithLatencyInfo(fakeEvent, latency_info);
    // If this key event was handled by the input method, but
    // -doCommandBySelector: (invoked by the call to -interpretKeyEvents: above)
    // enqueued edit commands, then in order to let webkit handle them
    // correctly, we need to send the real key event and corresponding edit
    // commands after processing the input method result.
    // We shouldn't do this if a new marked text was set by the input method,
    // otherwise the new marked text might be cancelled by webkit.
    if (hasEditCommands_ && !hasMarkedText_)
      delayEventUntilAfterImeCompostion = YES;
  } else {
    widgetHost->ForwardKeyboardEventWithCommands(event, latency_info,
                                                 &editCommands_);
  }

  // Calling ForwardKeyboardEventWithCommands() could have destroyed the
  // widget. When the widget was destroyed,
  // |renderWidgetHostView_->host()| will be set to NULL. So we
  // check it here and return immediately if it's NULL.
  if (!renderWidgetHostView_->host())
    return;

  // Then send keypress and/or composition related events.
  // If there was a marked text or the text to be inserted is longer than 1
  // character, then we send the text by calling FinishComposingText().
  // Otherwise, if the text to be inserted only contains 1 character, then we
  // can just send a keypress event which is fabricated by changing the type of
  // the keydown event, so that we can retain all necessary informations, such
  // as unmodifiedText, etc. And we need to set event.skip_in_browser to true to
  // prevent the browser from handling it again.
  // Note that, |textToBeInserted_| is a UTF-16 string, but it's fine to only
  // handle BMP characters here, as we can always insert non-BMP characters as
  // text.
  BOOL textInserted = NO;
  if (textToBeInserted_.length() >
      ((hasMarkedText_ || oldHasMarkedText) ? 0u : 1u)) {
    widgetHost->ImeCommitText(textToBeInserted_, std::vector<ui::ImeTextSpan>(),
                              gfx::Range::InvalidRange(), 0);
    textInserted = YES;
  }

  // Updates or cancels the composition. If some text has been inserted, then
  // we don't need to cancel the composition explicitly.
  if (hasMarkedText_ && markedText_.length()) {
    // Sends the updated marked text to the renderer so it can update the
    // composition node in WebKit.
    // When marked text is available, |markedTextSelectedRange_| will be the
    // range being selected inside the marked text.
    widgetHost->ImeSetComposition(markedText_, ime_text_spans_,
                                  setMarkedTextReplacementRange_,
                                  markedTextSelectedRange_.location,
                                  NSMaxRange(markedTextSelectedRange_));
  } else if (oldHasMarkedText && !hasMarkedText_ && !textInserted) {
    if (unmarkTextCalled_) {
      widgetHost->ImeFinishComposingText(false);
    } else {
      widgetHost->ImeCancelComposition();
    }
  }

  // Clear information from |interpretKeyEvents:|
  setMarkedTextReplacementRange_ = gfx::Range::InvalidRange();

  // If the key event was handled by the input method but it also generated some
  // edit commands, then we need to send the real key event and corresponding
  // edit commands here. This usually occurs when the input method wants to
  // finish current composition session but still wants the application to
  // handle the key event. See http://crbug.com/48161 for reference.
  if (delayEventUntilAfterImeCompostion) {
    // If |delayEventUntilAfterImeCompostion| is YES, then a fake key down event
    // with windowsKeyCode == 0xE5 has already been sent to webkit.
    // So before sending the real key down event, we need to send a fake key up
    // event to balance it.
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.SetType(blink::WebInputEvent::kKeyUp);
    fakeEvent.skip_in_browser = true;
    ui::LatencyInfo fake_event_latency_info = latency_info;
    fake_event_latency_info.set_source_event_type(ui::SourceEventType::OTHER);
    widgetHost->ForwardKeyboardEventWithLatencyInfo(fakeEvent,
                                                    fake_event_latency_info);
    // Not checking |renderWidgetHostView_->host()| here because
    // a key event with |skip_in_browser| == true won't be handled by browser,
    // thus it won't destroy the widget.

    widgetHost->ForwardKeyboardEventWithCommands(event, fake_event_latency_info,
                                                 &editCommands_);

    // Calling ForwardKeyboardEventWithCommands() could have destroyed the
    // widget. When the widget was destroyed,
    // |renderWidgetHostView_->host()| will be set to NULL. So we
    // check it here and return immediately if it's NULL.
    if (!renderWidgetHostView_->host())
      return;
  }

  const NSUInteger kCtrlCmdKeyMask = NSControlKeyMask | NSCommandKeyMask;
  // Only send a corresponding key press event if there is no marked text.
  if (!hasMarkedText_) {
    if (!textInserted && textToBeInserted_.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      event.SetType(blink::WebInputEvent::kChar);
      event.text[0] = textToBeInserted_[0];
      event.text[1] = 0;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEventWithLatencyInfo(event, latency_info);
    } else if ((!textInserted || delayEventUntilAfterImeCompostion) &&
               event.text[0] != '\0' &&
               (([theEvent modifierFlags] & kCtrlCmdKeyMask) ||
                (hasEditCommands_ && editCommands_.empty()))) {
      // We don't get insertText: calls if ctrl or cmd is down, or the key event
      // generates an insert command. So synthesize a keypress event for these
      // cases, unless the key event generated any other command.
      event.SetType(blink::WebInputEvent::kChar);
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEventWithLatencyInfo(event, latency_info);
    }
  }

  // Possibly autohide the cursor.
  if ([self shouldAutohideCursorForEvent:theEvent]) {
    [NSCursor setHiddenUntilMouseMoves:YES];
    cursorHidden_ = YES;
  }
}

- (void)forceTouchEvent:(NSEvent*)theEvent {
  if (ui::ForceClickInvokesQuickLook())
    [self quickLookWithEvent:theEvent];
}

- (void)shortCircuitScrollWheelEvent:(NSEvent*)event {
  if ([event phase] != NSEventPhaseEnded &&
      [event phase] != NSEventPhaseCancelled) {
    return;
  }

  if (renderWidgetHostView_->host()) {
    // History-swiping is not possible if the logic reaches this point.
    WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(event, self);
    webEvent.rails_mode = mouseWheelFilter_.UpdateRailsMode(webEvent);
    if (renderWidgetHostView_->wheel_scroll_latching_enabled()) {
      renderWidgetHostView_->mouse_wheel_phase_handler_
          .AddPhaseIfNeededAndScheduleEndEvent(webEvent, false);
    } else {
      ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
      latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
      renderWidgetHostView_->host()->ForwardWheelEventWithLatencyInfo(
          webEvent, latency_info);
    }
  }

  if (endWheelMonitor_) {
    [NSEvent removeMonitor:endWheelMonitor_];
    endWheelMonitor_ = nil;
  }
}

- (void)handleBeginGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ beginGestureWithEvent:event];
  gestureBeginEvent_.reset(
      new WebGestureEvent(WebGestureEventBuilder::Build(event, self)));

  // If the page is at the minimum zoom level, require a threshold be reached
  // before the pinch has an effect.
  if (renderWidgetHostView_->page_at_minimum_scale_) {
    pinchHasReachedZoomThreshold_ = false;
    pinchUnusedAmount_ = 1;
  }
}

- (void)handleEndGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ endGestureWithEvent:event];

  // On macOS 10.11+, the end event has type = NSEventTypeMagnify and phase =
  // NSEventPhaseEnded. On macOS 10.10 and older, the event has type =
  // NSEventTypeEndGesture.
  if ([event type] == NSEventTypeMagnify ||
      [event type] == NSEventTypeEndGesture) {
    gestureBeginEvent_.reset();

    if (!renderWidgetHostView_->host())
      return;

    if (gestureBeginPinchSent_) {
      WebGestureEvent endEvent(WebGestureEventBuilder::Build(event, self));
      endEvent.SetType(WebInputEvent::kGesturePinchEnd);
      endEvent.SetSourceDevice(
          blink::WebGestureDevice::kWebGestureDeviceTouchpad);
      renderWidgetHostView_->SendGesturePinchEvent(&endEvent);
      gestureBeginPinchSent_ = NO;
    }
  }
}

- (void)beginGestureWithEvent:(NSEvent*)event {
  // This method must be handled when linking with the 10.10 SDK or earlier, or
  // when the app is running on 10.10 or earlier.  In other circumstances, the
  // event will be handled by |magnifyWithEvent:|, so this method should do
  // nothing.
  bool shouldHandle = true;
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  shouldHandle = base::mac::IsAtMostOS10_10();
#endif

  if (shouldHandle) {
    [self handleBeginGestureWithEvent:event];
  }
}

- (void)endGestureWithEvent:(NSEvent*)event {
  // This method must be handled when linking with the 10.10 SDK or earlier, or
  // when the app is running on 10.10 or earlier.  In other circumstances, the
  // event will be handled by |magnifyWithEvent:|, so this method should do
  // nothing.
  bool shouldHandle = true;
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  shouldHandle = base::mac::IsAtMostOS10_10();
#endif

  if (shouldHandle) {
    [self handleEndGestureWithEvent:event];
  }
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesEndedWithEvent:event];
}

- (void)smartMagnifyWithEvent:(NSEvent*)event {
  const WebGestureEvent& smartMagnifyEvent =
      WebGestureEventBuilder::Build(event, self);
  if (renderWidgetHostView_ && renderWidgetHostView_->host()) {
    renderWidgetHostView_->host()->ForwardGestureEvent(smartMagnifyEvent);
  }
}

- (void)showLookUpDictionaryOverlayInternal:(NSAttributedString*)string
                              baselinePoint:(NSPoint)baselinePoint
                                 targetView:(NSView*)view {
  if ([string length] == 0) {
    // The PDF plugin does not support getting the attributed string at point.
    // Until it does, use NSPerformService(), which opens Dictionary.app.
    // TODO(shuchen): Support GetStringAtPoint() & GetStringFromRange() for PDF.
    // See https://crbug.com/152438.
    NSString* text = nil;
    if (auto* selection = renderWidgetHostView_->GetTextSelection())
      text = base::SysUTF16ToNSString(selection->selected_text());

    if ([text length] == 0)
      return;
    scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;
    NSArray* types = [NSArray arrayWithObject:NSStringPboardType];
    [pasteboard->get() declareTypes:types owner:nil];
    if ([pasteboard->get() setString:text forType:NSStringPboardType])
      NSPerformService(@"Look Up in Dictionary", pasteboard->get());
    return;
  }
  NSPoint flippedBaselinePoint = {
      baselinePoint.x, [view frame].size.height - baselinePoint.y,
  };
  [view showDefinitionForAttributedString:string atPoint:flippedBaselinePoint];
}

- (void)showLookUpDictionaryOverlayFromRange:(NSRange)range
                                  targetView:(NSView*)targetView {
  content::RenderWidgetHostViewBase* focusedView =
      renderWidgetHostView_->GetFocusedViewForTextSelection();
  if (!focusedView)
    return;

  RenderWidgetHostImpl* widgetHost =
      RenderWidgetHostImpl::From(focusedView->GetRenderWidgetHost());
  if (!widgetHost)
    return;

  int32_t targetWidgetProcessId = widgetHost->GetProcess()->GetID();
  int32_t targetWidgetRoutingId = widgetHost->GetRoutingID();
  TextInputClientMac::GetInstance()->GetStringFromRange(
      widgetHost, range, ^(NSAttributedString* string, NSPoint baselinePoint) {
        if (!content::RenderWidgetHost::FromID(targetWidgetProcessId,
                                               targetWidgetRoutingId)) {
          // By the time we get here |widgetHost| might have been destroyed.
          // (See https://crbug.com/737032).
          return;
        }

        if (auto* rwhv = widgetHost->GetView()) {
          gfx::Point pointInRootView = rwhv->TransformPointToRootCoordSpace(
              gfx::Point(baselinePoint.x, baselinePoint.y));
          baselinePoint.x = pointInRootView.x();
          baselinePoint.y = pointInRootView.y();
        }
        [self showLookUpDictionaryOverlayInternal:string
                                    baselinePoint:baselinePoint
                                       targetView:targetView];
      });
}

- (void)showLookUpDictionaryOverlayAtPoint:(NSPoint)point {
  gfx::PointF rootPoint(point.x, NSHeight([self frame]) - point.y);
  gfx::PointF transformedPoint;
  if (!renderWidgetHostView_->host() ||
      !renderWidgetHostView_->host()->delegate() ||
      !renderWidgetHostView_->host()->delegate()->GetInputEventRouter())
    return;

  RenderWidgetHostImpl* widgetHost =
      renderWidgetHostView_->host()
          ->delegate()
          ->GetInputEventRouter()
          ->GetRenderWidgetHostAtPoint(renderWidgetHostView_, rootPoint,
                                       &transformedPoint);
  if (!widgetHost)
    return;

  int32_t targetWidgetProcessId = widgetHost->GetProcess()->GetID();
  int32_t targetWidgetRoutingId = widgetHost->GetRoutingID();
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      widgetHost, gfx::ToFlooredPoint(transformedPoint),
      ^(NSAttributedString* string, NSPoint baselinePoint) {
        if (!content::RenderWidgetHost::FromID(targetWidgetProcessId,
                                               targetWidgetRoutingId)) {
          // By the time we get here |widgetHost| might have been destroyed.
          // (See https://crbug.com/737032).
          return;
        }

        if (auto* rwhv = widgetHost->GetView()) {
          gfx::Point pointInRootView = rwhv->TransformPointToRootCoordSpace(
              gfx::Point(baselinePoint.x, baselinePoint.y));
          baselinePoint.x = pointInRootView.x();
          baselinePoint.y = pointInRootView.y();
        }
        [self showLookUpDictionaryOverlayInternal:string
                                    baselinePoint:baselinePoint
                                       targetView:self];
      });
}

// This is invoked only on 10.8 or newer when the user taps a word using
// three fingers.
- (void)quickLookWithEvent:(NSEvent*)event {
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  [self showLookUpDictionaryOverlayAtPoint:point];
}

// This method handles 2 different types of hardware events.
// (Apple does not distinguish between them).
//  a. Scrolling the middle wheel of a mouse.
//  b. Swiping on the track pad.
//
// This method is responsible for 2 types of behavior:
//  a. Scrolling the content of window.
//  b. Navigating forwards/backwards in history.
//
// This is a brief description of the logic:
//  1. If the content can be scrolled, scroll the content.
//     (This requires a roundtrip to blink to determine whether the content
//      can be scrolled.)
//     Once this logic is triggered, the navigate logic cannot be triggered
//     until the gesture finishes.
//  2. If the user is making a horizontal swipe, start the navigate
//     forward/backwards UI.
//     Once this logic is triggered, the user can either cancel or complete
//     the gesture. If the user completes the gesture, all remaining touches
//     are swallowed, and not allowed to scroll the content. If the user
//     cancels the gesture, all remaining touches are forwarded to the content
//     scroll logic. The user cannot trigger the navigation logic again.
- (void)scrollWheel:(NSEvent*)event {
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  // When linking against the 10.11 (or later) SDK and running on 10.11 or
  // later, check the phase of the event and specially handle the "begin" and
  // "end" phases.
  if (base::mac::IsAtLeastOS10_11()) {
    if (event.phase == NSEventPhaseBegan) {
      [self handleBeginGestureWithEvent:event];
    }

    if (event.phase == NSEventPhaseEnded ||
        event.phase == NSEventPhaseCancelled) {
      [self handleEndGestureWithEvent:event];
    }
  }
#endif

  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:event];
    if (handled)
      return;
  }

  // Compute Event.Latency.OS.MOUSE_WHEEL histogram.
  ui::ComputeEventLatencyOS(event);

  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the view
  // when the scrolling ends (e.g. if the tab was switched). This is necessary
  // for ending rubber-banding in such cases.
  if ([event phase] == NSEventPhaseBegan && !endWheelMonitor_) {
    endWheelMonitor_ = [NSEvent
        addLocalMonitorForEventsMatchingMask:NSScrollWheelMask
                                     handler:^(NSEvent* blockEvent) {
                                       [self shortCircuitScrollWheelEvent:
                                                 blockEvent];
                                       return blockEvent;
                                     }];
  }

  // This is responsible for content scrolling!
  if (renderWidgetHostView_->host()) {
    WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(event, self);
    webEvent.rails_mode = mouseWheelFilter_.UpdateRailsMode(webEvent);
    ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
    if (renderWidgetHostView_->wheel_scroll_latching_enabled()) {
      renderWidgetHostView_->mouse_wheel_phase_handler_
          .AddPhaseIfNeededAndScheduleEndEvent(
              webEvent, renderWidgetHostView_->ShouldRouteEvent(webEvent));
      if (webEvent.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
        // A wheel end event is scheduled and will get dispatched if momentum
        // phase doesn't start in 100ms. Don't sent the wheel end event
        // immediately.
        return;
      }
    }

    if (renderWidgetHostView_->ShouldRouteEvent(webEvent)) {
      renderWidgetHostView_->host()
          ->delegate()
          ->GetInputEventRouter()
          ->RouteMouseWheelEvent(renderWidgetHostView_, &webEvent,
                                 latency_info);
    } else {
      renderWidgetHostView_->ProcessMouseWheelEvent(webEvent, latency_info);
    }
  }
}

// Called repeatedly during a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
  if (!renderWidgetHostView_->host())
    return;

#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  // When linking against the 10.11 (or later) SDK and running on 10.11 or
  // later, check the phase of the event and specially handle the "begin" and
  // "end" phases.
  if (base::mac::IsAtLeastOS10_11()) {
    if (event.phase == NSEventPhaseBegan) {
      [self handleBeginGestureWithEvent:event];
      return;
    }

    if (event.phase == NSEventPhaseEnded ||
        event.phase == NSEventPhaseCancelled) {
      [self handleEndGestureWithEvent:event];
      return;
    }
  }
#endif

  // If this conditional evalutes to true, and the function has not
  // short-circuited from the previous block, then this event is a duplicate of
  // a gesture event, and should be ignored.
  if (event.phase == NSEventPhaseBegan || event.phase == NSEventPhaseEnded ||
      event.phase == NSEventPhaseCancelled) {
    return;
  }

  // If, due to nesting of multiple gestures (e.g, from multiple touch
  // devices), the beginning of the gesture has been lost, skip the remainder
  // of the gesture.
  if (!gestureBeginEvent_)
    return;

  if (!pinchHasReachedZoomThreshold_) {
    pinchUnusedAmount_ *= (1 + [event magnification]);
    if (pinchUnusedAmount_ < 0.667 || pinchUnusedAmount_ > 1.5)
      pinchHasReachedZoomThreshold_ = true;
  }

  // Send a GesturePinchBegin event if none has been sent yet.
  if (!gestureBeginPinchSent_) {
    if (renderWidgetHostView_->wheel_scroll_latching_enabled()) {
      // Before starting a pinch sequence, send the pending wheel end event to
      // finish scrolling.
      renderWidgetHostView_->mouse_wheel_phase_handler_
          .DispatchPendingWheelEndEvent();
    }
    WebGestureEvent beginEvent(*gestureBeginEvent_);
    beginEvent.SetType(WebInputEvent::kGesturePinchBegin);
    beginEvent.SetSourceDevice(
        blink::WebGestureDevice::kWebGestureDeviceTouchpad);
    renderWidgetHostView_->SendGesturePinchEvent(&beginEvent);
    gestureBeginPinchSent_ = YES;
  }

  // Send a GesturePinchUpdate event.
  WebGestureEvent updateEvent = WebGestureEventBuilder::Build(event, self);
  updateEvent.data.pinch_update.zoom_disabled = !pinchHasReachedZoomThreshold_;
  renderWidgetHostView_->SendGesturePinchEvent(&updateEvent);
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSWindow* oldWindow = [self window];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  if (oldWindow) {
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidChangeBackingPropertiesNotification
                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidMoveNotification
                                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidEndLiveResizeNotification
                                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidBecomeKeyNotification
                                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidResignKeyNotification
                                object:oldWindow];
  }
  if (newWindow) {
    [notificationCenter
        addObserver:self
           selector:@selector(windowDidChangeBackingProperties:)
               name:NSWindowDidChangeBackingPropertiesNotification
             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowChangedGlobalFrame:)
                               name:NSWindowDidMoveNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowChangedGlobalFrame:)
                               name:NSWindowDidEndLiveResizeNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowDidBecomeKey:)
                               name:NSWindowDidBecomeKeyNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowDidResignKey:)
                               name:NSWindowDidResignKeyNotification
                             object:newWindow];
  }
}

- (void)updateScreenProperties {
  renderWidgetHostView_->UpdateNSViewAndDisplayProperties();
  renderWidgetHostView_->UpdateDisplayLink();
}

// http://developer.apple.com/library/mac/#documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/CapturingScreenContents/CapturingScreenContents.html#//apple_ref/doc/uid/TP40012302-CH10-SW4
- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  // Background tabs check if their screen scale factor, color profile, and
  // vsync properties changed when they are added to a window.

  // Allocating a CGLayerRef with the current scale factor immediately from
  // this handler doesn't work. Schedule the backing store update on the
  // next runloop cycle, then things are read for CGLayerRef allocations to
  // work.
  [self performSelector:@selector(updateScreenProperties)
             withObject:nil
             afterDelay:0];
}

- (void)windowChangedGlobalFrame:(NSNotification*)notification {
  renderWidgetHostView_->UpdateNSViewAndDisplayProperties();
}

- (void)setFrameSize:(NSSize)newSize {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::setFrameSize");

  // NB: -[NSView setFrame:] calls through -setFrameSize:, so overriding
  // -setFrame: isn't neccessary.
  [super setFrameSize:newSize];

  renderWidgetHostView_->UpdateNSViewAndDisplayProperties();

  // Wait for the frame that WasResize might have requested. If the view is
  // being made visible at a new size, then this call will have no effect
  // because the view widget is still hidden, and the pause call in WasShown
  // will have this effect for us.
  renderWidgetHostView_->PauseForPendingResizeOrRepaintsAndDraw();
}

- (BOOL)canBecomeKeyView {
  if (!renderWidgetHostView_->host())
    return NO;

  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  if (!renderWidgetHostView_->host())
    return NO;

  return canBeKeyView_;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  DCHECK([self window]);
  DCHECK_EQ([self window], [notification object]);
  if ([responderDelegate_ respondsToSelector:@selector(windowDidBecomeKey)])
    [responderDelegate_ windowDidBecomeKey];
  if ([self window].isKeyWindow && [[self window] firstResponder] == self)
    renderWidgetHostView_->SetActive(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  DCHECK([self window]);
  DCHECK_EQ([self window], [notification object]);

  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [self window]))
    return;

  if ([[self window] firstResponder] == self)
    renderWidgetHostView_->SetActive(false);
}

- (BOOL)becomeFirstResponder {
  if (!renderWidgetHostView_->host())
    return NO;
  if ([responderDelegate_ respondsToSelector:@selector(becomeFirstResponder)])
    [responderDelegate_ becomeFirstResponder];

  renderWidgetHostView_->host()->GotFocus();
  renderWidgetHostView_->SetTextInputActive(true);

  // Cancel any onging composition text which was left before we lost focus.
  // TODO(suzhe): We should do it in -resignFirstResponder: method, but
  // somehow that method won't be called when switching among different tabs.
  // See http://crbug.com/47209
  [self cancelComposition];

  NSNumber* direction = [NSNumber
      numberWithUnsignedInteger:[[self window] keyViewSelectionDirection]];
  NSDictionary* userInfo =
      [NSDictionary dictionaryWithObject:direction forKey:kSelectionDirection];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kViewDidBecomeFirstResponder
                    object:self
                  userInfo:userInfo];

  return YES;
}

- (BOOL)resignFirstResponder {
  if ([responderDelegate_ respondsToSelector:@selector(resignFirstResponder)])
    [responderDelegate_ resignFirstResponder];
  renderWidgetHostView_->SetTextInputActive(false);
  if (!renderWidgetHostView_->host())
    return YES;

  if (closeOnDeactivate_)
    renderWidgetHostView_->KillSelf();

  renderWidgetHostView_->host()->LostFocus();

  // We should cancel any onging composition whenever RWH's Blur() method gets
  // called, because in this case, webkit will confirm the ongoing composition
  // internally.
  [self cancelComposition];

  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector
                          (validateUserInterfaceItem:isValidItem:)]) {
    BOOL valid;
    BOOL known =
        [responderDelegate_ validateUserInterfaceItem:item isValidItem:&valid];
    if (known)
      return valid;
  }

  SEL action = [item action];
  BOOL is_render_view =
      RenderViewHost::From(renderWidgetHostView_->host()) != nullptr;

  if (action == @selector(stopSpeaking:))
    return is_render_view && ui::TextServicesContextMenu::IsSpeaking();

  if (action == @selector(startSpeaking:))
    return is_render_view;

  // For now, these actions are always enabled for render view,
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(undo:) || action == @selector(redo:) ||
      action == @selector(cut:) || action == @selector(copy:) ||
      action == @selector(copyToFindPboard:) || action == @selector(paste:) ||
      action == @selector(pasteAndMatchStyle:)) {
    return is_render_view;
  }

  return editCommand_helper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return renderWidgetHostView_;
}

// Determine whether we should autohide the cursor (i.e., hide it until mouse
// move) for the given event. Customize here to be more selective about which
// key presses to autohide on.
- (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event {
  return (renderWidgetHostView_->GetTextInputType() !=
              ui::TEXT_INPUT_TYPE_NONE &&
          [event type] == NSKeyDown &&
          !([event modifierFlags] & NSCommandKeyMask))
             ? YES
             : NO;
}

- (NSArray*)accessibilityArrayAttributeValues:(NSString*)attribute
                                        index:(NSUInteger)index
                                     maxCount:(NSUInteger)maxCount {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  NSUInteger totalLength = [fullArray count];
  if (index >= totalLength)
    return nil;
  NSUInteger length = MIN(totalLength - index, maxCount);
  return [fullArray subarrayWithRange:NSMakeRange(index, length)];
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->host()->GetRootBrowserAccessibilityManager();

  // Contents specifies document view of RenderWidgetHostViewCocoa provided by
  // BrowserAccessibilityManager. Children includes all subviews in addition to
  // contents. Currently we do not have subviews besides the document view.
  if (([attribute isEqualToString:NSAccessibilityChildrenAttribute] ||
       [attribute isEqualToString:NSAccessibilityContentsAttribute]) &&
      manager) {
    return [NSArray
        arrayWithObjects:ToBrowserAccessibilityCocoa(manager->GetRoot()), nil];
  } else if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return NSAccessibilityScrollAreaRole;
  }
  id ret = [super accessibilityAttributeValue:attribute];
  return ret;
}

- (NSArray*)accessibilityAttributeNames {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [ret addObject:NSAccessibilityContentsAttribute];
  [ret addObjectsFromArray:[super accessibilityAttributeNames]];
  return ret;
}

- (id)accessibilityHitTest:(NSPoint)point {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->host()->GetRootBrowserAccessibilityManager();
  if (!manager)
    return self;
  NSPoint pointInWindow =
      ui::ConvertPointFromScreenToWindow([self window], point);
  NSPoint localPoint = [self convertPoint:pointInWindow fromView:nil];
  localPoint.y = NSHeight([self bounds]) - localPoint.y;
  BrowserAccessibilityCocoa* root =
      ToBrowserAccessibilityCocoa(manager->GetRoot());
  id obj = [root accessibilityHitTest:localPoint];
  return obj;
}

- (BOOL)accessibilityIsIgnored {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->host()->GetRootBrowserAccessibilityManager();
  return !manager;
}

- (NSUInteger)accessibilityGetIndexOf:(id)child {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->host()->GetRootBrowserAccessibilityManager();
  // Only child is root.
  if (manager && ToBrowserAccessibilityCocoa(manager->GetRoot()) == child) {
    return 0;
  } else {
    return NSNotFound;
  }
}

- (id)accessibilityFocusedUIElement {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->host()->GetRootBrowserAccessibilityManager();
  if (manager) {
    BrowserAccessibility* focused_item = manager->GetFocus();
    DCHECK(focused_item);
    if (focused_item) {
      BrowserAccessibilityCocoa* focused_item_cocoa =
          ToBrowserAccessibilityCocoa(focused_item);
      DCHECK(focused_item_cocoa);
      if (focused_item_cocoa)
        return focused_item_cocoa;
    }
  }
  return [super accessibilityFocusedUIElement];
}

// Below is our NSTextInputClient implementation.
//
// When WebHTMLView receives a NSKeyDown event, WebHTMLView calls the following
// functions to process this event.
//
// [WebHTMLView keyDown] ->
//     EventHandler::keyEvent() ->
//     ...
//     [WebEditorClient handleKeyboardEvent] ->
//     [WebHTMLView _interceptEditingKeyEvent] ->
//     [NSResponder interpretKeyEvents] ->
//     [WebHTMLView insertText] ->
//     Editor::insertText()
//
// Unfortunately, it is hard for Chromium to use this implementation because
// it causes key-typing jank.
// RenderWidgetHostViewMac is running in a browser process. On the other
// hand, Editor and EventHandler are running in a renderer process.
// So, if we used this implementation, a NSKeyDown event is dispatched to
// the following functions of Chromium.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Sync IPC (KeyDown)| (*1) ->
//     EventHandler::keyEvent() (renderer) ->
//     ...
//     EditorClientImpl::handleKeyboardEvent() (renderer) ->
//     |Sync IPC| (*2) ->
//     [RenderWidgetHostViewMac _interceptEditingKeyEvent] (browser) ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC| ->
//     Editor::insertText() (renderer)
//
// (*1) we need to wait until this call finishes since WebHTMLView uses the
// result of EventHandler::keyEvent().
// (*2) we need to wait until this call finishes since WebEditorClient uses
// the result of [WebHTMLView _interceptEditingKeyEvent].
//
// This needs many sync IPC messages sent between a browser and a renderer for
// each key event, which would probably result in key-typing jank.
// To avoid this problem, this implementation processes key events (and input
// method events) totally in a browser process and sends asynchronous input
// events, almost same as KeyboardEvents (and TextEvents) of DOM Level 3, to a
// renderer process.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Async IPC (RawKeyDown)| ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC (Char)| ->
//     Editor::insertText() (renderer)
//
// Since this implementation doesn't have to wait any IPC calls, this doesn't
// make any key-typing jank. --hbono 7/23/09
//
extern "C" {
extern NSString* NSTextInputReplacementRangeAttributeName;
}

- (NSArray*)validAttributesForMarkedText {
  // This code is just copied from WebKit except renaming variables.
  if (!validAttributesForMarkedText_) {
    validAttributesForMarkedText_.reset([[NSArray alloc]
        initWithObjects:NSUnderlineStyleAttributeName,
                        NSUnderlineColorAttributeName,
                        NSMarkedClauseSegmentAttributeName,
                        NSTextInputReplacementRangeAttributeName, nil]);
  }
  return validAttributesForMarkedText_.get();
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  DCHECK([self window]);
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.
  thePoint = ui::ConvertPointFromScreenToWindow([self window], thePoint);
  thePoint = [self convertPoint:thePoint fromView:nil];
  thePoint.y = NSHeight([self frame]) - thePoint.y;

  if (!renderWidgetHostView_->host() ||
      !renderWidgetHostView_->host()->delegate() ||
      !renderWidgetHostView_->host()->delegate()->GetInputEventRouter())
    return NSNotFound;

  gfx::PointF rootPoint(thePoint.x, thePoint.y);
  gfx::PointF transformedPoint;
  RenderWidgetHostImpl* widgetHost =
      renderWidgetHostView_->host()
          ->delegate()
          ->GetInputEventRouter()
          ->GetRenderWidgetHostAtPoint(renderWidgetHostView_, rootPoint,
                                       &transformedPoint);
  if (!widgetHost)
    return NSNotFound;

  NSUInteger index =
      TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
          widgetHost, gfx::ToFlooredPoint(transformedPoint));
  return index;
}

- (NSRect)firstViewRectForCharacterRange:(NSRange)theRange
                             actualRange:(NSRangePointer)actualRange {
  NSRect rect;
  if (!renderWidgetHostView_->GetCachedFirstRectForCharacterRange(
          theRange, &rect, actualRange)) {
    rect = TextInputClientMac::GetInstance()->GetFirstRectForRange(
        renderWidgetHostView_->GetFocusedWidget(), theRange);

    // TODO(thakis): Pipe |actualRange| through TextInputClientMac machinery.
    if (actualRange)
      *actualRange = theRange;
  }

  // The returned rectangle is in WebKit coordinates (upper left origin), so
  // flip the coordinate system.
  NSRect viewFrame = [self frame];
  rect.origin.y = NSHeight(viewFrame) - NSMaxY(rect);
  return rect;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer)actualRange {
  // During tab closure, events can arrive after RenderWidgetHostViewMac::
  // Destroy() is called, which will have set |host()| to null.
  if (!renderWidgetHostView_->GetFocusedWidget()) {
    [self cancelComposition];
    return NSZeroRect;
  }

  NSRect rect =
      [self firstViewRectForCharacterRange:theRange actualRange:actualRange];

  // Convert into screen coordinates for return.
  rect = [self convertRect:rect toView:nil];
  rect = [[self window] convertRectToScreen:rect];
  return rect;
}

- (NSRange)selectedRange {
  if (selectedRange_.location == NSNotFound)
    return NSMakeRange(NSNotFound, 0);
  return selectedRange_;
}

- (NSRange)markedRange {
  // An input method calls this method to check if an application really has
  // a text being composed when hasMarkedText call returns true.
  // Returns the range saved in the setMarkedText method so the input method
  // calls the setMarkedText method and we can update the composition node
  // there. (When this method returns an empty range, the input method doesn't
  // call the setMarkedText method.)
  return hasMarkedText_ ? markedRange_ : NSMakeRange(NSNotFound, 0);
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                   (NSRangePointer)actualRange {
  // Prepare |actualRange| as if the proposed range is invalid. If it is valid,
  // then |actualRange| will be updated again.
  if (actualRange)
    *actualRange = NSMakeRange(NSNotFound, 0);

  // The caller of this method is allowed to pass nonsensical ranges. These
  // can't even be converted into gfx::Ranges.
  if (range.location == NSNotFound || range.length == 0)
    return nil;
  if (range.length >= std::numeric_limits<NSUInteger>::max() - range.location)
    return nil;

  const gfx::Range requested_range(range);
  if (requested_range.is_reversed())
    return nil;

  gfx::Range expected_range;
  const base::string16* expected_text;
  const content::TextInputManager::CompositionRangeInfo* compositionInfo =
      renderWidgetHostView_->GetCompositionRangeInfo();
  const content::TextInputManager::TextSelection* selection =
      renderWidgetHostView_->GetTextSelection();
  if (!selection)
    return nil;

  if (compositionInfo && !compositionInfo->range.is_empty()) {
    // This method might get called after TextInputState.type is reset to none,
    // in which case there will be no composition range information
    // (https://crbug.com/698672).
    expected_text = &markedText_;
    expected_range = compositionInfo->range;
  } else {
    expected_text = &selection->text();
    size_t offset = selection->offset();
    expected_range = gfx::Range(offset, offset + expected_text->size());
  }

  gfx::Range actual_range = expected_range.Intersect(requested_range);
  if (!actual_range.IsValid())
    return nil;

  // Gets the raw bytes to avoid unnecessary string copies for generating
  // NSString.
  const base::char16* bytes =
      &(*expected_text)[actual_range.start() - expected_range.start()];
  // Avoid integer overflow.
  base::CheckedNumeric<size_t> requested_len = actual_range.length();
  requested_len *= sizeof(base::char16);
  NSUInteger bytes_len =
      base::strict_cast<NSUInteger, size_t>(requested_len.ValueOrDefault(0));
  base::scoped_nsobject<NSString> ns_string([[NSString alloc]
      initWithBytes:bytes
             length:bytes_len
           encoding:NSUTF16LittleEndianStringEncoding]);
  if (actualRange)
    *actualRange = actual_range.ToNSRange();

  return [[[NSAttributedString alloc] initWithString:ns_string] autorelease];
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

// Each RenderWidgetHostViewCocoa has its own input context, but we return
// nil when the caret is in non-editable content or password box to avoid
// making input methods do their work.
- (NSTextInputContext*)inputContext {
  switch (renderWidgetHostView_->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return nil;
    default:
      return [super inputContext];
  }
}

- (BOOL)hasMarkedText {
  // An input method calls this function to figure out whether or not an
  // application is really composing a text. If it is composing, it calls
  // the markedRange method, and maybe calls the setMarkedText method.
  // It seems an input method usually calls this function when it is about to
  // cancel an ongoing composition. If an application has a non-empty marked
  // range, it calls the setMarkedText method to delete the range.
  return hasMarkedText_;
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  hasMarkedText_ = NO;
  markedText_.clear();
  markedTextSelectedRange_ = NSMakeRange(NSNotFound, 0);
  ime_text_spans_.clear();

  // If we are handling a key down event, then FinishComposingText() will be
  // called in keyEvent: method.
  if (!handlingKeyDown_) {
    if (renderWidgetHostView_->GetActiveWidget()) {
      renderWidgetHostView_->GetActiveWidget()->ImeFinishComposingText(false);
    }
  } else {
    unmarkTextCalled_ = YES;
  }
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)newSelRange
     replacementRange:(NSRange)replacementRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];

  // |markedRange_| will get set on a callback from ImeSetComposition().
  markedTextSelectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);

  ime_text_spans_.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &ime_text_spans_);
  } else {
    // Use a thin black underline by default.
    ime_text_spans_.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, 0, length,
        ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  }

  // If we are handling a key down event, then SetComposition() will be
  // called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if (handlingKeyDown_) {
    setMarkedTextReplacementRange_ = gfx::Range(replacementRange);
  } else {
    if (renderWidgetHostView_->GetActiveWidget()) {
      renderWidgetHostView_->GetActiveWidget()->ImeSetComposition(
          markedText_, ime_text_spans_, gfx::Range(replacementRange),
          newSelRange.location, NSMaxRange(newSelRange));
    }
  }
}

- (void)doCommandBySelector:(SEL)selector {
  // An input method calls this function to dispatch an editing command to be
  // handled by this view.
  if (selector == @selector(noop:))
    return;

  std::string command(base::SysNSStringToUTF8(
      RenderWidgetHostViewMacEditCommandHelper::CommandNameForSelector(
          selector)));

  // If this method is called when handling a key down event, then we need to
  // handle the command in the key event handler. Otherwise we can just handle
  // it here.
  if (handlingKeyDown_) {
    hasEditCommands_ = YES;
    // We ignore commands that insert characters, because this was causing
    // strange behavior (e.g. tab always inserted a tab rather than moving to
    // the next field on the page).
    if (!base::StartsWith(command, "insert",
                          base::CompareCase::INSENSITIVE_ASCII))
      editCommands_.push_back(EditCommand(command, ""));
  } else {
    if (renderWidgetHostView_->host()->delegate()) {
      renderWidgetHostView_->host()->delegate()->ExecuteEditCommand(
          command, base::nullopt);
    }
  }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  // An input method has characters to be inserted.
  // Same as Linux, Mac calls this method not only:
  // * when an input method finishes composing text, but also;
  // * when we type an ASCII character (without using input methods).
  // When we aren't using input methods, we should send the given character as
  // a Char event so it is dispatched to an onkeypress() event handler of
  // JavaScript.
  // On the other hand, when we are using input methods, we should send the
  // given characters as an input method event and prevent the characters from
  // being dispatched to onkeypress() event handlers.
  // Text inserting might be initiated by other source instead of keyboard
  // events, such as the Characters dialog. In this case the text should be
  // sent as an input method event as well.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
  } else {
    gfx::Range replacement_range(replacementRange);
    if (renderWidgetHostView_->GetActiveWidget()) {
      renderWidgetHostView_->GetActiveWidget()->ImeCommitText(
          base::SysNSStringToUTF16(im_text), std::vector<ui::ImeTextSpan>(),
          replacement_range, 0);
    }
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

- (void)insertText:(id)string {
  [self insertText:string replacementRange:NSMakeRange(NSNotFound, 0)];
}

- (void)viewDidMoveToWindow {
  // This can be called very late during shutdown, so it needs to be guarded by
  // a check that DestroyCompositorForShutdown has not yet been called.
  // https://crbug.com/805726
  if (!renderWidgetHostView_->browser_compositor_)
    return;

  if ([self window])
    [self updateScreenProperties];
  renderWidgetHostView_->browser_compositor_->SetNSViewAttachedToWindow(
      [self window]);

  // If we switch windows (or are removed from the view hierarchy), cancel any
  // open mouse-downs.
  if (hasOpenMouseDown_) {
    WebMouseEvent event(WebInputEvent::kMouseUp, WebInputEvent::kNoModifiers,
                        ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
    event.button = WebMouseEvent::Button::kLeft;
    renderWidgetHostView_->ForwardMouseEvent(event);

    hasOpenMouseDown_ = NO;
  }
}

- (void)undo:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Undo();
}

- (void)redo:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Redo();
}

- (void)cut:(id)sender {
  if (auto* delegate =
          renderWidgetHostView_->GetFocusedRenderWidgetHostDelegate()) {
    delegate->Cut();
  }
}

- (void)copy:(id)sender {
  if (auto* delegate =
          renderWidgetHostView_->GetFocusedRenderWidgetHostDelegate()) {
    delegate->Copy();
  }
}

- (void)copyToFindPboard:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->CopyToFindPboard();
}

- (void)paste:(id)sender {
  if (auto* delegate =
          renderWidgetHostView_->GetFocusedRenderWidgetHostDelegate()) {
    delegate->Paste();
  }
}

- (void)pasteAndMatchStyle:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->PasteAndMatchStyle();
}

- (void)selectAll:(id)sender {
  // editCommand_helper_ adds implementations for most NSResponder methods
  // dynamically. But the renderer side only sends selection results back to
  // the browser if they were triggered by a keyboard event or went through
  // one of the Select methods on RWH. Since selectAll: is called from the
  // menu handler, neither is true.
  // Explicitly call SelectAll() here to make sure the renderer returns
  // selection results.
  if (auto* delegate =
          renderWidgetHostView_->GetFocusedRenderWidgetHostDelegate()) {
    delegate->SelectAll();
  }
}

- (void)startSpeaking:(id)sender {
  GetRenderWidgetHostViewToUse(renderWidgetHostView_)->SpeakSelection();
}

- (void)stopSpeaking:(id)sender {
  ui::TextServicesContextMenu::StopSpeaking();
}

- (void)cancelComposition {
  if (!hasMarkedText_)
    return;

  NSTextInputContext* inputContext = [self inputContext];
  [inputContext discardMarkedText];

  hasMarkedText_ = NO;
  // Should not call [self unmarkText] here, because it'll send unnecessary
  // cancel composition IPC message to the renderer.
}

- (void)finishComposingText {
  if (!hasMarkedText_)
    return;

  if (renderWidgetHostView_->GetActiveWidget()) {
    renderWidgetHostView_->GetActiveWidget()->ImeFinishComposingText(false);
  }

  [self cancelComposition];
}

// Overriding a NSResponder method to support application services.

- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  id requestor = nil;
  BOOL sendTypeIsString = [sendType isEqual:NSStringPboardType];
  BOOL returnTypeIsString = [returnType isEqual:NSStringPboardType];
  const content::TextInputManager::TextSelection* selection =
      renderWidgetHostView_->GetTextSelection();
  BOOL hasText = selection && !selection->selected_text().empty();
  BOOL takesText =
      renderWidgetHostView_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;

  if (sendTypeIsString && hasText && !returnType) {
    requestor = self;
  } else if (!sendType && returnTypeIsString && takesText) {
    requestor = self;
  } else if (sendTypeIsString && returnTypeIsString && hasText && takesText) {
    requestor = self;
  } else {
    requestor =
        [super validRequestorForSendType:sendType returnType:returnType];
  }
  return requestor;
}

- (void)updateCursor:(NSCursor*)cursor {
  if (currentCursor_ == cursor)
    return;

  currentCursor_.reset([cursor retain]);
  [[self window] invalidateCursorRectsForView:self];

  // NSWindow's invalidateCursorRectsForView: resets cursor rects but does not
  // update the cursor instantly. The cursor is updated when the mouse moves.
  // Update the cursor by setting the current cursor if not hidden.
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (!cursorHidden_ && web_contents && !web_contents->IsShowingContextMenu())
    [currentCursor_ set];
}

- (void)popupWindowWillClose:(NSNotification*)notification {
  renderWidgetHostView_->KillSelf();
}

@end

//
// Supporting application services
//
@implementation RenderWidgetHostViewCocoa (NSServicesRequests)

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  const content::TextInputManager::TextSelection* selection =
      renderWidgetHostView_->GetTextSelection();
  if (!selection || selection->selected_text().empty() ||
      ![types containsObject:NSStringPboardType]) {
    return NO;
  }

  base::scoped_nsobject<NSString> text([[NSString alloc]
      initWithUTF8String:base::UTF16ToUTF8(selection->selected_text())
                             .c_str()]);
  NSArray* toDeclare = [NSArray arrayWithObject:NSStringPboardType];
  [pboard declareTypes:toDeclare owner:nil];
  return [pboard setString:text forType:NSStringPboardType];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSString* string = [pboard stringForType:NSStringPboardType];
  if (!string)
    return NO;

  // If the user is currently using an IME, confirm the IME input,
  // and then insert the text from the service, the same as TextEdit and Safari.
  [self finishComposingText];
  [self insertText:string];
  return YES;
}

// "-webkit-app-region: drag | no-drag" is implemented on Mac by excluding
// regions that are not draggable. (See ControlRegionView in
// native_app_window_cocoa.mm). This requires the render host view to be
// draggable by default.
- (BOOL)mouseDownCanMoveWindow {
  return YES;
}

@end
