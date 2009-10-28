// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/bubble_border.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Browser;
class ExtensionHost;

class ExtensionPopup : public BrowserBubble,
                       public NotificationObserver,
                       public ExtensionView::Container {
 public:
  virtual ~ExtensionPopup();

  // Create and show a popup with |url| positioned below |relative_to| in
  // screen coordinates. This is anchored to the lower middle of the rect,
  // extending to the left, just like the wrench and page menus.
  //
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static ExtensionPopup* Show(const GURL& url, Browser* browser,
                              const gfx::Rect& relative_to);

  ExtensionHost* host() const { return extension_host_.get(); }

  // BrowserBubble overrides.
  virtual void Hide();
  virtual void Show();
  virtual void ResizeToView();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ExtensionView::Container overrides.
  virtual void OnExtensionMouseEvent(ExtensionView* view) { };
  virtual void OnExtensionMouseLeave(ExtensionView* view) { };
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view);

  // The min/max height of popups.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 private:
  ExtensionPopup(ExtensionHost* host,
                 views::Widget* frame,
                 const gfx::Rect& relative_to);

  // The area on the screen that the popup should be positioned relative to.
  gfx::Rect relative_to_;

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  NotificationRegistrar registrar_;

  // A separate widget and associated pieces to draw a border behind the
  // popup.  This has to be a separate window in order to support transparency.
  // Layered windows can't contain native child windows, so we wouldn't be
  // able to have the ExtensionView child.
  views::Widget* border_widget_;
  BubbleBorder* border_;
  views::View* border_view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_H_
