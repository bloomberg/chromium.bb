// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/histogram.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_trial.h"
#import "chrome/browser/cocoa/rwhvm_editcommand_helper.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/render_messages.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/mac/WebInputEventFactory.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/glue/webmenurunner_mac.h"

using WebKit::WebInputEventFactory;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

@interface RenderWidgetHostViewCocoa (Private)
+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event;
- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r;
- (void)cancelChildPopups;
@end

namespace {

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

}

// RenderWidgetHostView --------------------------------------------------------

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget)
    : render_widget_host_(widget),
      about_to_validate_and_paint_(false),
      im_attributes_(nil),
      im_composing_(false),
      is_loading_(false),
      is_hidden_(false),
      shutdown_factory_(this),
      parent_view_(NULL) {
  // |cocoa_view_| owns us and we will be deleted when |cocoa_view_| goes away.
  // Since we autorelease it, our caller must put |native_view()| into the view
  // hierarchy right after calling us.
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];
  render_widget_host_->set_view(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
  [im_attributes_ release];
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  [parent_host_view->GetNativeView() addSubview:cocoa_view_];
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:activatable_ ? YES : NO];

  // TODO(avi):Why the hell are these screen coordinates? The Windows code calls
  // ::MoveWindow() which indicates they should be local, but when running it I
  // get global ones instead!

  NSPoint global_origin = NSPointFromCGPoint(pos.origin().ToCGPoint());
  global_origin.y = [[[cocoa_view_ window] screen] frame].size.height -
      pos.height() - global_origin.y;
  NSPoint window_origin =
      [[cocoa_view_ window] convertScreenToBase:global_origin];
  NSPoint view_origin =
      [cocoa_view_ convertPoint:window_origin fromView:nil];
  NSRect initial_frame = NSMakeRect(view_origin.x,
                                    view_origin.y,
                                    pos.width(),
                                    pos.height());
  [cocoa_view_ setFrame:initial_frame];
}

RenderWidgetHost* RenderWidgetHostViewMac::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewMac::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  render_widget_host_->WasRestored();
}

void RenderWidgetHostViewMac::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  render_widget_host_->WasHidden();
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  if (is_hidden_)
    return;

  // TODO(avi): the TabContents object uses this method to size the newly
  // created widget to the correct size. For that instance, we're not yet in the
  // view hierarchy so |size| ends up being 0x0. However, this works for us
  // because we're using the Cocoa view struture and resizer flags to fix things
  // up as soon as the view gets added to the hierarchy. Figure out if we want
  // to keep this flow or switch back to the flow Windows uses which sets the
  // size upon creation. <http://crbug.com/8285>. However, if the size is not
  // 0x0, this is a valid request.
  if (size.IsEmpty())
    return;

  // Do conversions to upper-left origin, as "set size" means keep the
  // upper-left corner pinned. If the new size is valid, this is a popup whose
  // superview is another RenderWidgetHostViewCocoa, but even if it's directly
  // in a TabContentsViewCocoa, they're both BaseViews.
  gfx::Rect rect =
      [(BaseView*)[cocoa_view_ superview] NSRectToRect:[cocoa_view_ frame]];
  rect.set_width(size.width());
  rect.set_height(size.height());
  [cocoa_view_ setFrame:[(BaseView*)[cocoa_view_ superview] RectToNSRect:rect]];
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() {
  return native_view();
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const std::vector<webkit_glue::WebPluginGeometry>& moves) {
  // All plugin stuff is TBD. TODO(avi,awalker): fill in
  // http://crbug.com/8192
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

void RenderWidgetHostViewMac::Blur() {
  [[cocoa_view_ window] makeFirstResponder:nil];
}

bool RenderWidgetHostViewMac::HasFocus() {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

void RenderWidgetHostViewMac::Show() {
  [cocoa_view_ setHidden:NO];

  DidBecomeSelected();
}

void RenderWidgetHostViewMac::Hide() {
  [cocoa_view_ setHidden:YES];

  WasHidden();
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() const {
  return [cocoa_view_ NSRectToRect:[cocoa_view_ bounds]];
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::UpdateCursorIfOverSelf() {
  // Do something special (as Win Chromium does) for arrow cursor while loading
  // a page? TODO(avi): decide
  // Can we synchronize to the event stream? Switch to -[NSWindow
  // mouseLocationOutsideOfEventStream] if we cannot. TODO(avi): test and see
  NSEvent* event = [[cocoa_view_ window] currentEvent];
  if ([event window] != [cocoa_view_ window])
    return;

  NSPoint event_location = [event locationInWindow];
  NSPoint local_point = [cocoa_view_ convertPoint:event_location fromView:nil];

  if (!NSPointInRect(local_point, [cocoa_view_ bounds]))
    return;

  NSCursor* ns_cursor = current_cursor_.GetCursor();
  [ns_cursor set];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  // The renderer updates its IME status.
  // We need to control the input method according to the given message.

  // We need to convert the coordinate of the cursor rectangle sent from the
  // renderer and save it. Our IME backend uses a coordinate system whose
  // origin is the upper-left corner of this view. On the other hand, Cocoa
  // uses a coordinate system whose origin is the lower-left corner of this
  // view. So, we convert the cursor rectangle and save it.
  NSRect view_rect = [cocoa_view_ bounds];
  const int y_offset = static_cast<int>(view_rect.size.height);
  im_caret_rect_ = NSMakeRect(caret_rect.x(),
                              y_offset - caret_rect.y() - caret_rect.height(),
                              caret_rect.width(), caret_rect.height());
}

void RenderWidgetHostViewMac::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  NSRect ns_rect = [cocoa_view_ RectToNSRect:rect];

  if (about_to_validate_and_paint_) {
    // As much as we'd like to use -setNeedsDisplayInRect: here, we can't. We're
    // in the middle of executing a -drawRect:, and as soon as it returns Cocoa
    // will clear its record of what needs display. If we want to handle the
    // recursive drawing, we need to do it ourselves.
    invalid_rect_ = NSUnionRect(invalid_rect_, ns_rect);
  } else {
    [cocoa_view_ setNeedsDisplayInRect:ns_rect];
    [cocoa_view_ displayIfNeeded];
  }
}

void RenderWidgetHostViewMac::DidScrollRect(
    const gfx::Rect& rect, int dx, int dy) {
  if (is_hidden_)
    return;

  // We've already modified the BackingStore to reflect the scroll, so
  // simply ask the RWHVCocoa to redraw itself based on the new
  // pixels.  We cannot use -[NSView scrollRect:by:] here because the
  // findbar and blocked popups will leave trails behind.
  // TODO(rohitrao): Evaluate how slow this full redraw is.  If it
  // turns out to be a problem, consider scrolling only a portion of
  // the view, based on where the findbar and blocked popups are.
  DidPaintRect(rect);
}

void RenderWidgetHostViewMac::RenderViewGone() {
  // TODO(darin): keep this around, and draw sad-tab into it.
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // On Windows, popups are implemented with a popup window style, so that when
  // an event comes in that would "cancel" it, it receives the OnCancelMode
  // message and can kill itself. Alas, on the Mac, views cannot capture events
  // outside of themselves. On Windows, if Destroy is being called on a view,
  // then the event causing the destroy had also cancelled any popups by the
  // time Destroy() was called. On the Mac we have to destroy all the popups
  // ourselves.

  // Depth-first destroy all popups. Use ShutdownHost() to enforce deepest-first
  // ordering.
  for (RenderWidgetHostViewCocoa* subview in [cocoa_view_ subviews]) {
    [subview renderWidgetHostViewMac]->ShutdownHost();
  }

  // We've been told to destroy.
  [cocoa_view_ retain];
  [cocoa_view_ removeFromSuperview];
  [cocoa_view_ autorelease];

  // We get this call just before |render_widget_host_| deletes
  // itself.  But we are owned by |cocoa_view_|, which may be retained
  // by some other code.  Examples are TabContentsViewMac's
  // |latent_focus_view_| and TabWindowController's
  // |cachedContentView_|.
  render_widget_host_ = NULL;
}

// Called from the renderer to tell us what the tooltip text should be. It
// calls us frequently so we need to cache the value to prevent doing a lot
// of repeat work.
void RenderWidgetHostViewMac::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_ && [[cocoa_view_ window] isKeyWindow]) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite. Don't persist the trimmed
    // string, as then the comparison above will always fail and we'll try to
    // set it again every single time the mouse moves.
    std::wstring display_text = tooltip_text_;
    if (tooltip_text_.length() > kMaxTooltipLength)
      display_text = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysWideToNSString(display_text);
    [cocoa_view_ setToolTipAtMousePoint:tooltip_nsstring];
  }
}

BackingStore* RenderWidgetHostViewMac::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(render_widget_host_, size);
}

// Display a popup menu for WebKit using Cocoa widgets.
void RenderWidgetHostViewMac::ShowPopupWithItems(
    gfx::Rect bounds,
    int item_height,
    int selected_item,
    const std::vector<WebMenuItem>& items) {
  NSRect view_rect = [cocoa_view_ bounds];
  NSRect parent_rect = [parent_view_ bounds];
  int y_offset = bounds.y() + bounds.height();
  NSRect position = NSMakeRect(bounds.x(), parent_rect.size.height - y_offset,
                               bounds.width(), bounds.height());

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items]);

  [menu_runner runMenuInView:parent_view_
                  withBounds:position
                initialIndex:selected_item];

  int window_num = [[parent_view_ window] windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Simulate a menu selection event.
    const WebMouseEvent& mouse_event =
        WebInputEventFactory::mouseEvent(event, cocoa_view_);
    render_widget_host_->ForwardMouseEvent(mouse_event);
  } else {
    // Simulate a menu dismiss event.
    NativeWebKeyboardEvent keyboard_event(event);
    render_widget_host_->ForwardKeyboardEvent(keyboard_event);
  }
}

void RenderWidgetHostViewMac::KillSelf() {
  if (shutdown_factory_.empty()) {
    [cocoa_view_ setHidden:YES];
    MessageLoop::current()->PostTask(FROM_HERE,
        shutdown_factory_.NewRunnableMethod(
            &RenderWidgetHostViewMac::ShutdownHost));
  }
}

void RenderWidgetHostViewMac::ShutdownHost() {
  shutdown_factory_.RevokeAll();
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

namespace {

// Adjusts an NSRect in screen coordinates to have an origin in the upper left,
// and stuffs it into a gfx::Rect. This is likely incorrect for a multiple-
// monitor setup.
gfx::Rect NSRectToRect(const NSRect rect, NSScreen* screen) {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y([screen frame].size.height - new_rect.y() - new_rect.height());
  return new_rect;
}

}  // namespace

gfx::Rect RenderWidgetHostViewMac::GetWindowRect() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  if (!cocoa_view_ || ![cocoa_view_ window]) {
    return gfx::Rect();
  }

  NSRect bounds = [cocoa_view_ bounds];
  bounds = [cocoa_view_ convertRect:bounds toView:nil];
  bounds.origin = [[cocoa_view_ window] convertBaseToScreen:bounds.origin];
  return NSRectToRect(bounds, [[cocoa_view_ window] screen]);
}

gfx::Rect RenderWidgetHostViewMac::GetRootWindowRect() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  if (!cocoa_view_ || ![cocoa_view_ window]) {
    return gfx::Rect();
  }

  NSRect bounds = [[cocoa_view_ window] frame];
  return NSRectToRect(bounds, [[cocoa_view_ window] screen]);
}

void RenderWidgetHostViewMac::SetActive(bool active) {
  if (render_widget_host_)
    render_widget_host_->SetActive(active);
}

void RenderWidgetHostViewMac::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  if (render_widget_host_)
    render_widget_host_->Send(new ViewMsg_SetBackground(
        render_widget_host_->routing_id(), background));
}

// EditCommandMatcher ---------------------------------------------------------

// This class is used to capture the shortcuts that a given key event maps to.
// We instantiate a vanilla NSResponder, call interpretKeyEvents on it, and
// record all of the selectors passed into doCommandBySelector while
// interpreting the key event.  The selectors are converted into edit commands
// which can be passed to the render process.
//
// Caveats:
// - Shortcuts involving a sequence of key combinations (chords) don't work,
// because we instantiate a new responder for each event.
// - We ignore key combinations that don't include a modifier (ctrl, cmd, alt)
// because this was causing strange behavior (e.g. tab always inserted a tab
// rather than moving to the next field on the page).

@interface EditCommandMatcher : NSResponder {
  EditCommands* edit_commands_;
}
@end

@implementation EditCommandMatcher

- (id)initWithEditCommands:(EditCommands*)edit_commands {
  if ((self = [super init]) != nil) {
    edit_commands_ = edit_commands;
  }
  return self;
}

- (void)doCommandBySelector:(SEL)selector {
  NSString* editCommand =
      RWHVMEditCommandHelper::CommandNameForSelector(selector);
  edit_commands_->push_back(
      EditCommand(base::SysNSStringToUTF8(editCommand), ""));
}

- (void)insertText:(id)string {
  // If we don't ignore this, then sometimes we get a bell.
}

+ (void)matchEditCommands:(EditCommands*)edit_commands
                 forEvent:(NSEvent*)theEvent {
  if ([theEvent type] != NSKeyDown)
    return;
  // Don't interpret plain key presses.  This screws up things like <Tab>.
  NSUInteger flags = [theEvent modifierFlags];
  flags &= (NSControlKeyMask | NSAlternateKeyMask | NSCommandKeyMask);
  if (!flags)
    return;
  scoped_nsobject<EditCommandMatcher> matcher(
      [[EditCommandMatcher alloc] initWithEditCommands:edit_commands]);
  [matcher.get() interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
}

@end

// RenderWidgetHostViewCocoa ---------------------------------------------------

@implementation RenderWidgetHostViewCocoa

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    editCommand_helper_.reset(new RWHVMEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    renderWidgetHostView_ = r;
    canBeKeyView_ = YES;
    closeOnDeactivate_ = NO;
  }
  return self;
}

- (void)dealloc {
  delete renderWidgetHostView_;
  [toolTip_ release];

  [super dealloc];
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  // Don't cancel child popups; killing them on a mouse click would prevent the
  // user from positioning the insertion point in the text field spawning the
  // popup. A click outside the text field would cause the text field to drop
  // the focus, and then EditorClientImpl::textFieldDidEndEditing() would cancel
  // the popup anyway, so we're OK.

  const WebMouseEvent& event =
      WebInputEventFactory::mouseEvent(theEvent, self);
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardMouseEvent(event);
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  // We have some magic in |CrApplication sendEvent:| that always sends key 
  // events to |keyEvent:| so that cocoa doesn't have a chance to intercept it.
  DCHECK([[self window] firstResponder] != self);
  return NO;
}

- (void)keyEvent:(NSEvent*)theEvent {
  // TODO(avi): Possibly kill self? See RenderWidgetHostViewWin::OnKeyEvent and
  // http://b/issue?id=1192881 .

  // Don't cancel child popups; the key events are probably what's triggering
  // the popup in the first place.

  NativeWebKeyboardEvent event(theEvent);

  // Save the modifier keys so the insertText method can use it when it sends
  // a Char event, which is dispatched as an onkeypress() event of JavaScript.
  renderWidgetHostView_->im_modifiers_ = event.modifiers;

  // To emulate Windows, over-write |event.windowsKeyCode| to VK_PROCESSKEY
  // while an input method is composing a text.
  // Gmail checks this code in its onkeydown handler to stop auto-completing
  // e-mail addresses while composing a CJK text.
  if ([theEvent type] == NSKeyDown && renderWidgetHostView_->im_composing_) {
    event.windowsKeyCode = 0xE5;  // VKEY_PROCESSKEY
    event.setKeyIdentifierFromWindowsKeyCode();
    event.skip_in_browser = true;
  }

  // Dispatch this keyboard event to the renderer.
  if (renderWidgetHostView_->render_widget_host_) {
    RenderWidgetHost* widgetHost = renderWidgetHostView_->render_widget_host_;
    // Look up shortcut, if any, for this key combination.
    EditCommands editCommands;
    [EditCommandMatcher matchEditCommands:&editCommands forEvent:theEvent];
    if (!editCommands.empty())
      widgetHost->ForwardEditCommandsForNextKeyEvent(editCommands);
    widgetHost->ForwardKeyboardEvent(event);
  }

  // Dispatch a NSKeyDown event to an input method.
  // To send an onkeydown() event before an onkeypress() event, we should
  // dispatch this NSKeyDown event AFTER sending it to the renderer.
  // (See <https://bugs.webkit.org/show_bug.cgi?id=25119>).
  if ([theEvent type] == NSKeyDown)
    [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];

  // Possibly autohide the cursor.
  if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
    [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)scrollWheel:(NSEvent *)theEvent {
  [self cancelChildPopups];

  const WebMouseWheelEvent& event =
      WebInputEventFactory::mouseWheelEvent(theEvent, self);
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(event);
}

// See the comment in RenderWidgetHostViewMac::Destroy() about cancellation
// events. On the Mac we must kill popups on outside events, thus this lovely
// case of filicide caused by events on parent views.
- (void)cancelChildPopups {
  // If this view can be the key view, it is not a popup. Therefore, if it has
  // any children, they are popups that need to be canceled.
  if (canBeKeyView_) {
    for (RenderWidgetHostViewCocoa* subview in [self subviews]) {
      subview->renderWidgetHostView_->KillSelf();
    }
  }
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->WasResized();
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!renderWidgetHostView_->render_widget_host_) {
    // TODO(shess): Consider using something more noticable?
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    return;
  }

  DCHECK(renderWidgetHostView_->render_widget_host_->process()->HasConnection());
  DCHECK(!renderWidgetHostView_->about_to_validate_and_paint_);

  renderWidgetHostView_->invalid_rect_ = dirtyRect;
  renderWidgetHostView_->about_to_validate_and_paint_ = true;
  BackingStore* backing_store =
      renderWidgetHostView_->render_widget_host_->GetBackingStore(true);
  renderWidgetHostView_->about_to_validate_and_paint_ = false;
  dirtyRect = renderWidgetHostView_->invalid_rect_;

  if (backing_store) {
    NSRect view_bounds = [self bounds];
    gfx::Rect damaged_rect([self NSRectToRect:dirtyRect]);

    gfx::Rect bitmap_rect(0, 0,
                          backing_store->size().width(),
                          backing_store->size().height());

    // Specify the proper y offset to ensure that the view is rooted to the
    // upper left corner.  This can be negative, if the window was resized
    // smaller and the renderer hasn't yet repainted.
    int yOffset = NSHeight(view_bounds) - backing_store->size().height();

    gfx::Rect paint_rect = bitmap_rect.Intersect(damaged_rect);
    if (!paint_rect.IsEmpty()) {
      // if we have a CGLayer, draw that into the window
      if (backing_store->cg_layer()) {
        CGContextRef context = static_cast<CGContextRef>(
            [[NSGraphicsContext currentContext] graphicsPort]);

        // TODO: add clipping to dirtyRect if it improves drawing performance.
        CGContextDrawLayerAtPoint(context, CGPointMake(0.0, yOffset),
                                  backing_store->cg_layer());
      } else {
        // if we haven't created a layer yet, draw the cached bitmap into
        // the window.  The CGLayer will be created the next time the renderer
        // paints.
        CGContextRef context = static_cast<CGContextRef>(
            [[NSGraphicsContext currentContext] graphicsPort]);
        scoped_cftyperef<CGImageRef> image(
            CGBitmapContextCreateImage(backing_store->cg_bitmap()));
        CGRect imageRect = bitmap_rect.ToCGRect();
        imageRect.origin.y = yOffset;
        CGContextDrawImage(context, imageRect, image);
      }
    }

    // Fill the remaining portion of the damaged_rect with white
    if (damaged_rect.right() > bitmap_rect.right()) {
      int x = std::max(bitmap_rect.right(), damaged_rect.x());
      int y = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      int width = damaged_rect.right() - x;
      int height = damaged_rect.y() - y;

      // Extra fun to get around the fact that gfx::Rects can't have
      // negative sizes.
      if (width < 0) {
        x += width;
        width = -width;
      }
      if (height < 0) {
        y += height;
        height = -height;
      }

      NSRect r = [self RectToNSRect:gfx::Rect(x, y, width, height)];
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      int x = damaged_rect.x();
      int y = damaged_rect.bottom();
      int width = damaged_rect.right() - x;
      int height = std::max(bitmap_rect.bottom(), damaged_rect.y()) - y;

      // Extra fun to get around the fact that gfx::Rects can't have
      // negative sizes.
      if (width < 0) {
        x += width;
        width = -width;
      }
      if (height < 0) {
        y += height;
        height = -height;
      }

      NSRect r = [self RectToNSRect:gfx::Rect(x, y, width, height)];
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (!renderWidgetHostView_->whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks();
    }
  } else {
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    if (renderWidgetHostView_->whiteout_start_time_.is_null())
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks::Now();
  }
}

- (BOOL)canBecomeKeyView {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)becomeFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  renderWidgetHostView_->render_widget_host_->Focus();
  return YES;
}

- (BOOL)resignFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return YES;

  if (closeOnDeactivate_)
    renderWidgetHostView_->KillSelf();

  renderWidgetHostView_->render_widget_host_->Blur();

  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];

  return editCommand_helper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return renderWidgetHostView_;
}

// Determine whether we should autohide the cursor (i.e., hide it until mouse
// move) for the given event. Customize here to be more selective about which
// key presses to autohide on.
+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event {
  return ([event type] == NSKeyDown) ? YES : NO;
}

// Spellchecking methods
// The next three methods are implemented here since this class is the first
// responder for anything in the browser.

// This message is sent whenever the user specifies that a word should be
// changed from the spellChecker.
- (void)changeSpelling:(id)sender {
  // Grab the currently selected word from the spell panel, as this is the word
  // that we want to replace the selected word in the text with.
  NSString* newWord = [[sender selectedCell] stringValue];
  if (newWord != nil) {
    RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
    thisHostView->GetRenderWidgetHost()->Replace(
        base::SysNSStringToUTF16(newWord));
  }
}

// This message is sent by NSSpellChecker whenever the next word should be
// advanced to, either after a correction or clicking the "Find Next" button.
// This isn't documented anywhere useful, like in NSSpellProtocol.h with the
// other spelling panel methods. This is probably because Apple assumes that the
// the spelling panel will be used with an NSText, which will automatically
// catch this and advance to the next word for you. Thanks Apple.
- (void)checkSpelling:(id)sender {
  RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
  thisHostView->GetRenderWidgetHost()->AdvanceToNextMisspelling();
}

// This message is sent by the spelling panel whenever a word is ignored.
- (void)ignoreSpelling:(id)sender {
  // Ideally, we would ask the current RenderView for its tag, but that would
  // mean making a blocking IPC call from the browser. Instead,
  // SpellCheckerPlatform::CheckSpelling remembers the last tag and
  // SpellCheckerPlatform::IgnoreWord assumes that is the correct tag.
  NSString* wordToIgnore = [sender stringValue];
  if (wordToIgnore != nil) {
    SpellCheckerPlatform::IgnoreWord(base::SysNSStringToUTF16(wordToIgnore));

    // Strangely, the spellingPanel doesn't send checkSpelling after a word is
    // ignored, so we have to explicitly call AdvanceToNextMisspelling here.
    RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
    thisHostView->GetRenderWidgetHost()->AdvanceToNextMisspelling();
  }
}

- (void)showGuessPanel:(id)sender {
  RenderWidgetHostViewMac* thisHostView = [self renderWidgetHostViewMac];
  thisHostView->GetRenderWidgetHost()->ToggleSpellPanel(
      SpellCheckerPlatform::SpellingPanelVisible());
}

// END Spellchecking methods

// Below is the nasty tooltip stuff -- copied from WebKit's WebHTMLView.mm
// with minor modifications for code style and commenting.
//
//  The 'public' interface is -setToolTipAtMousePoint:. This differs from
// -setToolTip: in that the updated tooltip takes effect immediately,
//  without the user's having to move the mouse out of and back into the view.
//
// Unfortunately, doing this requires sending fake mouseEnter/Exit events to
// the view, which in turn requires overriding some internal tracking-rect
// methods (to keep track of its owner & userdata, which need to be filled out
// in the fake events.) --snej 7/6/09


/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *           (C) 2006, 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Any non-zero value will do, but using something recognizable might help us
// debug some day.
static const NSTrackingRectTag kTrackingRectTag = 0xBADFACE;

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (NSTrackingRectTag)addTrackingRect:(NSRect)rect
                               owner:(id)owner
                            userData:(void *)data
                        assumeInside:(BOOL)assumeInside {
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect
                                owner:(id)owner
                             userData:(void *)data
                         assumeInside:(BOOL)assumeInside
                       useTrackingNum:(int)tag {
  DCHECK(tag == 0 || tag == kTrackingRectTag);
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (void)_addTrackingRects:(NSRect *)rects
                    owner:(id)owner
             userDataList:(void **)userDataList
         assumeInsideList:(BOOL *)assumeInsideList
             trackingNums:(NSTrackingRectTag *)trackingNums
                    count:(int)count {
  DCHECK(count == 1);
  DCHECK(trackingNums[0] == 0 || trackingNums[0] == kTrackingRectTag);
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = userDataList[0];
  trackingNums[0] = kTrackingRectTag;
}

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (void)removeTrackingRect:(NSTrackingRectTag)tag {
  if (tag == 0)
    return;

  if (tag == kTrackingRectTag) {
    trackingRectOwner_ = nil;
    return;
  }

  if (tag == lastToolTipTag_) {
    [super removeTrackingRect:tag];
    lastToolTipTag_ = 0;
    return;
  }

  // If any other tracking rect is being removed, we don't know how it was
  // created and it's possible there's a leak involved (see Radar 3500217).
  NOTREACHED();
}

// Override of (apparently) a private NSView method(!)
- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count {
  for (int i = 0; i < count; ++i) {
    int tag = tags[i];
    if (tag == 0)
      continue;
    DCHECK(tag == kTrackingRectTag);
    trackingRectOwner_ = nil;
  }
}

// Sends a fake NSMouseExited event to the view for its current tracking rect.
- (void)_sendToolTipMouseExited {
  // Nothing matters except window, trackingNumber, and userData.
  int windowNumber = [[self window] windowNumber];
  NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
                                              location:NSMakePoint(0, 0)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:windowNumber
                                               context:NULL
                                           eventNumber:0
                                        trackingNumber:kTrackingRectTag
                                              userData:trackingRectUserData_];
  [trackingRectOwner_ mouseExited:fakeEvent];
}

// Sends a fake NSMouseEntered event to the view for its current tracking rect.
- (void)_sendToolTipMouseEntered {
  // Nothing matters except window, trackingNumber, and userData.
  int windowNumber = [[self window] windowNumber];
  NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
                                              location:NSMakePoint(0, 0)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:windowNumber
                                               context:NULL
                                           eventNumber:0
                                        trackingNumber:kTrackingRectTag
                                              userData:trackingRectUserData_];
  [trackingRectOwner_ mouseEntered:fakeEvent];
}

// Sets the view's current tooltip, to be displayed at the current mouse
// location. (This does not make the tooltip appear -- as usual, it only
// appears after a delay.) Pass null to remove the tooltip.
- (void)setToolTipAtMousePoint:(NSString *)string {
  NSString *toolTip = [string length] == 0 ? nil : string;
  NSString *oldToolTip = toolTip_;
  if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip
                                    : [toolTip isEqualToString:oldToolTip]) {
    return;
  }
  if (oldToolTip) {
    [self _sendToolTipMouseExited];
    [oldToolTip release];
  }
  toolTip_ = [toolTip copy];
  if (toolTip) {
    // See radar 3500217 for why we remove all tooltips
    // rather than just the single one we created.
    [self removeAllToolTips];
    NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
    lastToolTipTag_ = [self addToolTipRect:wideOpenRect
                                     owner:self
                                  userData:NULL];
    [self _sendToolTipMouseEntered];
  }
}

// NSView calls this to get the text when displaying the tooltip.
- (NSString *)view:(NSView *)view
  stringForToolTip:(NSToolTipTag)tag
             point:(NSPoint)point
          userData:(void *)data {
  return [[toolTip_ copy] autorelease];
}

// Below is our NSTextInput implementation.
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
// To avoid this problem, this implementation processes key events (and IME
// events) totally in a browser process and sends asynchronous input events,
// almost same as KeyboardEvents (and TextEvents) of DOM Level 3, to a
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
extern NSString *NSTextInputReplacementRangeAttributeName;
}

- (NSArray *)validAttributesForMarkedText {
  // This code is just copied from WebKit except renaming variables.
  if (!renderWidgetHostView_->im_attributes_) {
    renderWidgetHostView_->im_attributes_ = [[NSArray alloc] initWithObjects:
        NSUnderlineStyleAttributeName,
        NSUnderlineColorAttributeName,
        NSMarkedClauseSegmentAttributeName,
        NSTextInputReplacementRangeAttributeName,
        nil];
  }
  return renderWidgetHostView_->im_attributes_;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  NOTIMPLEMENTED();
  return NSNotFound;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange {
  // An input method requests a cursor rectangle to display its candidate
  // window.
  // Calculate the screen coordinate of the cursor rectangle saved in
  // RenderWidgetHostViewMac::IMEUpdateStatus() and send it to the IME.
  // Since this window may be moved since we receive the cursor rectangle last
  // time we sent the cursor rectangle to the IME, so we should map from the
  // view coordinate to the screen coordinate every time when an IME need it.
  NSRect resultRect = renderWidgetHostView_->im_caret_rect_;
  resultRect = [self convertRect:resultRect toView:nil];
  NSWindow* window = [self window];
  if (window)
    resultRect.origin = [window convertBaseToScreen:resultRect.origin];
  return resultRect;
}

- (NSRange)selectedRange {
  // Return the selected range saved in the setMarkedText method.
  return renderWidgetHostView_->im_selected_range_;
}

- (NSRange)markedRange {
  // An input method calls this method to check if an application really has
  // a text being composed when hasMarkedText call returns true.
  // Returns the range saved in the setMarkedText method so the input method
  // calls the setMarkedText method and we can update the composition node
  // there. (When this method returns an empty range, the input method doesn't
  // call the setMarkedText method.)
  return renderWidgetHostView_->im_marked_range_;
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)nsRange {
  // TODO(hbono): Even though many input method works without implementing
  // this method, we need to save a copy of the string in the setMarkedText
  // method and create a NSAttributedString with the given range.
  NOTIMPLEMENTED();
  return nil;
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

- (BOOL)hasMarkedText {
  // An input method calls this function to figure out whether or not an
  // application is really composing a text. If it is composing, it calls
  // the markedRange method, and maybe calls the setMarkedTest method.
  // It seems an input method usually calls this function when it is about to
  // cancel an ongoing composition. If an application has a non-empty marked
  // range, it calls the setMarkedText method to delete the range.
  return renderWidgetHostView_->im_composing_ ? YES : NO;
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  renderWidgetHostView_->render_widget_host_->ImeCancelComposition();
  renderWidgetHostView_->im_composing_ = false;
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];
  int cursor;
  int target_start;
  int target_end;
  if (!newSelRange.length) {
    // The given text doesn't have any range to be highlighted.
    // Put the cursor to the end of this text and clear the selection range.
    cursor = length;
    target_start = 0;
    target_end = 0;
  } else {
    // The given text has a range to be highlighted.
    // Set the selection range to the given one and put the cursor at the
    // beginning of the selection range.
    cursor = newSelRange.location;
    target_start = newSelRange.location;
    target_end = newSelRange.location + newSelRange.length;
  }

  // Dispatch this IME event to the renderer and update the IME state of this
  // object.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the IME state. (Our IME backend can automatically
  // cancels an ongoing composition when we send an empty text. So, it is OK
  // to send an empty text to the renderer.)
  renderWidgetHostView_->render_widget_host_->ImeSetComposition(
      UTF8ToUTF16([im_text UTF8String]), cursor, target_start, target_end);
  renderWidgetHostView_->GetRenderWidgetHost()->ImeSetInputMode(true);
  renderWidgetHostView_->im_composing_ = length > 0;
  renderWidgetHostView_->im_marked_range_.location = 0;
  renderWidgetHostView_->im_marked_range_.length = length;
  renderWidgetHostView_->im_selected_range_.location = newSelRange.location;
  renderWidgetHostView_->im_selected_range_.length = newSelRange.length;
}

- (void)doCommandBySelector:(SEL)selector {
  // An input method calls this function to dispatch an editing command to be
  // handled by this view.
  // Even though most editing commands has been already handled by the
  // RWHVMEditCommandHelper object, we need to handle an insertNewline: command
  // and send a '\r' character to WebKit so that WebKit dispatches this
  // character to onkeypress() event handlers.
  // TODO(hbono): need to handle more commands?
  if (selector == @selector(insertNewline:)) {
    NativeWebKeyboardEvent event('\r', renderWidgetHostView_->im_modifiers_,
                                 base::Time::Now().ToDoubleT());
    renderWidgetHostView_->render_widget_host_->ForwardKeyboardEvent(event);
  }
}

- (void)insertText:(id)string {
  // An input method has characters to be inserted.
  // Same as Linux, Mac calls this method not only:
  // * when an input method finishs composing text, but also;
  // * when we type an ASCII character (without using input methods).
  // When we aren't using input methods, we should send the given character as
  // a Char event so it is dispatched to an onkeypress() event handler of
  // JavaScript.
  // On the other hand, when we are using input methods, we should send the
  // given characters as an IME event and prevent the characters from being
  // dispatched to onkeypress() event handlers.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (!renderWidgetHostView_->im_composing_ && [im_text length] == 1) {
    NativeWebKeyboardEvent event([im_text characterAtIndex:0],
                                 renderWidgetHostView_->im_modifiers_,
                                 base::Time::Now().ToDoubleT());
    renderWidgetHostView_->render_widget_host_->ForwardKeyboardEvent(event);
  } else {
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        UTF8ToUTF16([im_text UTF8String]));
  }
  renderWidgetHostView_->im_composing_ = false;
}

@end

