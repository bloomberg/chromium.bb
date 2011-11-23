// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/views/browser_bubble.h"
#include "chrome/browser/ui/views/extensions/extension_view.h"
#include "content/public/browser/notification_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/views/bubble/bubble_border.h"


class Browser;
class ExtensionHost;

class ExtensionPopup : public BrowserBubble,
                       public BrowserBubble::Delegate,
                       public ExtensionView::Container,
                       public content::NotificationObserver,
                       public base::RefCounted<ExtensionPopup> {
 public:
  // Observer to ExtensionPopup events.
  class Observer {
   public:
    // Called when the ExtensionPopup is closing. Note that it
    // is ref-counted, and thus will be released shortly after
    // making this delegate call.
    virtual void ExtensionPopupIsClosing(ExtensionPopup* popup) {}
  };

  virtual ~ExtensionPopup();

  // Create and show a popup with |url| positioned adjacent to |relative_to| in
  // screen coordinates.
  // |browser| is the browser to which the pop-up will be attached.  NULL is a
  // valid parameter for pop-ups not associated with a browser.
  // The positioning of the pop-up is determined by |arrow_location| according
  // to the following logic:  The popup is anchored so that the corner indicated
  // by value of |arrow_location| remains fixed during popup resizes.
  // If |arrow_location| is BOTTOM_*, then the popup 'pops up', otherwise
  // the popup 'drops down'.
  // Pass |inspect_with_devtools| as true to pin the popup open and show the
  // devtools window for it.
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static ExtensionPopup* Show(const GURL& url,
                              Browser* browser,
                              const gfx::Rect& relative_to,
                              views::BubbleBorder::ArrowLocation arrow_location,
                              bool inspect_with_devtools,
                              Observer* observer);

  // Closes the ExtensionPopup.
  void Close();

  // Some clients wish to do their own custom focus change management. If this
  // is set to false, then the ExtensionPopup will not do anything in response
  // to the BubbleLostFocus() calls it gets from the BrowserBubble.
  void set_close_on_lost_focus(bool close_on_lost_focus) {
    close_on_lost_focus_ = close_on_lost_focus;
  }

  ExtensionHost* host() const { return extension_host_.get(); }

  // BrowserBubble overrides.
  virtual void Show(bool activate) OVERRIDE;

  // BrowserBubble::Delegate methods.
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble) OVERRIDE;
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble) OVERRIDE;
  virtual void BubbleGotFocus(BrowserBubble* bubble) OVERRIDE;
  virtual void BubbleLostFocus(BrowserBubble* bubble,
                               bool lost_focus_to_child) OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionView::Container overrides.
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view) OVERRIDE;

  // The min/max height of popups.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 private:
  ExtensionPopup(Browser* browser,
                 ExtensionHost* host,
                 const gfx::Rect& relative_to,
                 views::BubbleBorder::ArrowLocation arrow_location,
                 bool inspect_with_devtools,
                 Observer* observer);

  // The area on the screen that the popup should be positioned relative to.
  gfx::Rect relative_to_;

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  // Flag used to indicate if the pop-up should open a devtools window once
  // it is shown inspecting it.
  bool inspect_with_devtools_;

  // If false, ignore BrowserBubble::Delegate::BubbleLostFocus() calls.
  bool close_on_lost_focus_;

  // Whether the ExtensionPopup is current going about closing itself.
  bool closing_;

  content::NotificationRegistrar registrar_;

  // The observer of this popup.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
