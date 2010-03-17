// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/extensions/extension_view.h"
#include "chrome/browser/views/bubble_border.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "gfx/native_widget_types.h"
#include "googleurl/src/gurl.h"

class Browser;
class ExtensionHost;
class Profile;

namespace views {
class Widget;
}

class ExtensionPopup : public BrowserBubble,
                       public BrowserBubble::Delegate,
                       public NotificationObserver,
                       public ExtensionView::Container,
                       public base::RefCounted<ExtensionPopup> {
 public:
  // Observer to ExtensionPopup events.
  class Observer {
   public:
    // Called when the ExtensionPopup has closed. Note that it
    // is ref-counted, and thus will be released shortly after
    // making this delegate call.
    virtual void ExtensionPopupClosed(ExtensionPopup* popup) {}
  };

  enum PopupChrome {
    BUBBLE_CHROME,
    RECTANGLE_CHROME
  };

  virtual ~ExtensionPopup();

  // Create and show a popup with |url| positioned adjacent to |relative_to| in
  // screen coordinates.
  // |browser| is the browser to which the pop-up will be attached.  NULL is a
  // valid parameter for pop-ups not associated with a browser.
  // |profile| is the user profile instance associated with the popup.  A
  // non NULL value must be given.
  // |frame_window| is the native window that hosts the view inside which the
  // popup will be anchored.
  // The positioning of the pop-up is determined by |arrow_location| according
  // to the following logic:  The popup is anchored so that the corner indicated
  // by value of |arrow_location| remains fixed during popup resizes.
  // If |arrow_location| is BOTTOM_*, then the popup 'pops up', otherwise
  // the popup 'drops down'.
  // Pass |activate_on_show| as true to activate the popup window.
  // Pass |inspect_with_devtools| as true to pin the popup open and show the
  // devtools window for it.
  // The |chrome| argument controls the chrome that surrounds the pop-up.
  // Passing BUBBLE_CHROME will give the pop-up a bubble-like appearance,
  // including the arrow mentioned above.  Passing RECTANGLE_CHROME will give
  // the popup a rectangular, black border with a drop-shadow with no arrow.
  // The positioning of the popup is still governed by the arrow-location
  // parameter.
  //
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static ExtensionPopup* Show(const GURL& url, Browser* browser,
                              Profile* profile,
                              gfx::NativeWindow frame_window,
                              const gfx::Rect& relative_to,
                              BubbleBorder::ArrowLocation arrow_location,
                              bool activate_on_show,
                              bool inspect_with_devtools,
                              PopupChrome chrome,
                              Observer* observer);

  // Closes the ExtensionPopup (this will cause the delegate
  // ExtensionPopupClosed to fire.
  void Close();

  // Some clients wish to do their own custom focus change management. If this
  // is set to false, then the ExtensionPopup will not do anything in response
  // to the BubbleLostFocus() calls it gets from the BrowserBubble.
  void set_close_on_lost_focus(bool close_on_lost_focus) {
    close_on_lost_focus_ = close_on_lost_focus;
  }

  ExtensionHost* host() const { return extension_host_.get(); }

  // BrowserBubble overrides.
  virtual void Hide();
  virtual void Show(bool activate);
  virtual void ResizeToView();

  // BrowserBubble::Delegate methods.
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble);
  virtual void BubbleGotFocus(BrowserBubble* bubble);
  virtual void BubbleLostFocus(BrowserBubble* bubble,
                               bool lost_focus_to_child);

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ExtensionView::Container overrides.
  virtual void OnExtensionMouseEvent(ExtensionView* view) { }
  virtual void OnExtensionMouseLeave(ExtensionView* view) { }
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view);

  // The min/max height of popups.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 private:
  ExtensionPopup(ExtensionHost* host,
                 views::Widget* frame,
                 const gfx::Rect& relative_to,
                 BubbleBorder::ArrowLocation arrow_location,
                 bool activate_on_show,
                 bool inspect_with_devtools,
                 PopupChrome chrome,
                 Observer* observer);

  // Gives the desired bounds (in screen coordinates) given the rect to point
  // to and the size of the contained contents.  Includes all of the
  // border-chrome surrounding the pop-up as well.
  gfx::Rect GetOuterBounds(const gfx::Rect& position_relative_to,
                           const gfx::Size& contents_size) const;

  // The area on the screen that the popup should be positioned relative to.
  gfx::Rect relative_to_;

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  // Flag used to indicate if the pop-up should be activated upon first display.
  bool activate_on_show_;

  // Flag used to indicate if the pop-up should open a devtools window once
  // it is shown inspecting it.
  bool inspect_with_devtools_;

  // If false, ignore BrowserBubble::Delegate::BubbleLostFocus() calls.
  bool close_on_lost_focus_;

  // Whether the ExtensionPopup is current going about closing itself.
  bool closing_;

  NotificationRegistrar registrar_;

  // A separate widget and associated pieces to draw a border behind the
  // popup.  This has to be a separate window in order to support transparency.
  // Layered windows can't contain native child windows, so we wouldn't be
  // able to have the ExtensionView child.
  views::Widget* border_widget_;
  BubbleBorder* border_;
  views::View* border_view_;

  // The type of chrome associated with the popup window.
  PopupChrome popup_chrome_;

  // The observer of this popup.
  Observer* observer_;

  // A cached copy of the arrow-position for the bubble chrome.
  // If a black-border was requested, we still need this value to determine
  // the position of the pop-up in relation to |relative_to_|.
  BubbleBorder::ArrowLocation anchor_position_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
