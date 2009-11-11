// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/cocoa/base_view.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webmenuitem.h"

class RenderWidgetHostViewMac;
class RWHVMEditCommandHelper;
@class ToolTip;

@protocol RenderWidgetHostViewMacOwner
- (RenderWidgetHostViewMac*)renderWidgetHostViewMac;
@end

// This is the view that lives in the Cocoa view hierarchy. In Windows-land,
// RenderWidgetHostViewWin is both the view and the delegate. We split the roles
// but that means that the view needs to own the delegate and will dispose of it
// when it's removed from the view system.

@interface RenderWidgetHostViewCocoa
    : BaseView <RenderWidgetHostViewMacOwner, NSTextInput, NSChangeSpelling> {
 @private
  RenderWidgetHostViewMac* renderWidgetHostView_;  // Owned by us.
  BOOL canBeKeyView_;
  BOOL closeOnDeactivate_;
  scoped_ptr<RWHVMEditCommandHelper> editCommand_helper_;

  // These are part of the magic tooltip code from WebKit's WebHTMLView:
  id trackingRectOwner_;              // (not retained)
  void *trackingRectUserData_;
  NSTrackingRectTag lastToolTipTag_;
  NSString* toolTip_;

  BOOL ignoreKeyEvents_;
  scoped_nsobject<NSEvent> lastKeyPressedEvent_;
}

- (void)setCanBeKeyView:(BOOL)can;
- (void)setCloseOnDeactivate:(BOOL)b;
- (void)setToolTipAtMousePoint:(NSString *)string;

// When a keyboard event comes back from the renderer, we redispatch it. This
// makes sure we ignore it if we should receive it during redispatch, instead
// of sending it to the renderer again.
- (void)setIgnoreKeyEvents:(BOOL)ignorekeyEvents;
@end

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, and integrating with
//  the Cocoa view system. It is the implementation of the RenderWidgetHostView
//  that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHost* is tied to the render process.
//     If the render process dies, the RenderWidgetHost* goes away and all
//     references to it must become NULL."
//
class RenderWidgetHostViewMac : public RenderWidgetHostView {
 public:
  // The view will associate itself with the given widget. The native view must
  // be hooked up immediately to the view hierarchy, or else when it is
  // deleted it will delete this out from under the caller.
  explicit RenderWidgetHostViewMac(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewMac();

  RenderWidgetHostViewCocoa* native_view() const { return cocoa_view_; }

  // Implementation of RenderWidgetHostView:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos);
  virtual RenderWidgetHost* GetRenderWidgetHost() const;
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void SetSize(const gfx::Size& size);
  virtual gfx::NativeView GetNativeView();
  virtual void MovePluginWindows(
      const std::vector<webkit_glue::WebPluginGeometry>& moves);
  virtual void Focus();
  virtual void Blur();
  virtual bool HasFocus();
  virtual void Show();
  virtual void Hide();
  virtual gfx::Rect GetViewBounds() const;
  virtual void UpdateCursor(const WebCursor& cursor);
  virtual void SetIsLoading(bool is_loading);
  virtual void IMEUpdateStatus(int control, const gfx::Rect& caret_rect);
  virtual void DidPaintRect(const gfx::Rect& rect);
  virtual void DidScrollRect(const gfx::Rect& rect, int dx, int dy);
  virtual void RenderViewGone();
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) {};
  virtual void Destroy();
  virtual void SetTooltipText(const std::wstring& tooltip_text);
  virtual BackingStore* AllocBackingStore(const gfx::Size& size);
  virtual void ShowPopupWithItems(gfx::Rect bounds,
                                  int item_height,
                                  int selected_item,
                                  const std::vector<WebMenuItem>& items);
  virtual gfx::Rect GetWindowRect();
  virtual gfx::Rect GetRootWindowRect();
  virtual void SetActive(bool active);
  virtual void SetBackground(const SkBitmap& background);

  void KillSelf();

  void set_parent_view(BaseView* parent_view) { parent_view_ = parent_view; }

  // These member variables should be private, but the associated ObjC class
  // needs access to them and can't be made a friend.

  // The associated Model.  Can be NULL if Destroy() is called when
  // someone (other than superview) has retained |cocoa_view_|.
  RenderWidgetHost* render_widget_host_;

  // This is true when we are currently painting and thus should handle extra
  // paint requests by expanding the invalid rect rather than actually painting.
  bool about_to_validate_and_paint_;

  // This is the rectangle which we'll paint.
  NSRect invalid_rect_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks tab_switch_paint_time_;

  // Variables used by our implementaion of the NSTextInput protocol.
  // An input method of Mac calls the methods of this protocol not only to
  // notify an application of its status, but also to retrieve the status of
  // the application. That is, an application cannot control an input method
  // directly.
  // This object keeps the status of a composition of the renderer and returns
  // it when an input method asks for it.
  // We need to implement Objective-C methods for the NSTextInput protocol. On
  // the other hand, we need to implement a C++ method for an IPC-message
  // handler which receives input-method events from the renderer.
  // To avoid fragmentation of variables used by our input-method
  // implementation, we define all variables as public member variables of
  // this C++ class so both the C++ methods and the Objective-C methods can
  // access them.

  // Represents the input-method attributes supported by this object.
  NSArray* im_attributes_;

  // Represents whether or not an input method is composing a text.
  bool im_composing_;

  // Represents the range of the composition string (i.e. a text being
  // composed by an input method), and the range of the selected text of the
  // composition string.
  // TODO(hbono): need to save the composition string itself for the
  // attributedSubstringFromRange method?
  NSRange im_marked_range_;
  NSRange im_selected_range_;

  // Represents the state of modifier keys.
  // An input method doesn't notify the state of modifier keys. On the other
  // hand, the state of modifier keys are required by Char events because they
  // are dispatched to onkeypress() event handlers of JavaScript.
  // To create a Char event in NSTextInput methods, we save the latest state
  // of modifier keys when we receive it.
  int im_modifiers_;

  // Represents the cursor position in this view coordinate.
  // The renderer sends the cursor position through an IPC message.
  // We save the latest cursor position here and return it when an input
  // methods needs it.
  NSRect im_caret_rect_;

 private:
  // Updates the display cursor to the current cursor if the cursor is over this
  // render view.
  void UpdateCursorIfOverSelf();

  // Shuts down the render_widget_host_.  This is a separate function so we can
  // invoke it from the message loop.
  void ShutdownHost();

  // The associated view.
  RenderWidgetHostViewCocoa* cocoa_view_;  // WEAK

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Indicates if the page is loading.
  bool is_loading_;

  // true if the View is not visible.
  bool is_hidden_;

  // The text to be shown in the tooltip, supplied by the renderer.
  std::wstring tooltip_text_;

  // Factory used to safely scope delayed calls to ShutdownHost().
  ScopedRunnableMethodFactory<RenderWidgetHostViewMac> shutdown_factory_;

  // Used for positioning a popup menu.
  BaseView* parent_view_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMac);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
