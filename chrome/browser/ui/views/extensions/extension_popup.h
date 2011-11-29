// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/views/extensions/extension_view.h"
#include "content/public/browser/notification_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/views/bubble/bubble_delegate.h"

class Browser;

class ExtensionPopup : public views::BubbleDelegateView,
                       public ExtensionView::Container,
                       public content::NotificationObserver {
 public:
  virtual ~ExtensionPopup();

  // Create and show a popup with |url| positioned adjacent to |anchor_view|.
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
  static ExtensionPopup* ShowPopup(
      const GURL& url,
      Browser* browser,
      views::View* anchor_view,
      views::BubbleBorder::ArrowLocation arrow_location,
      bool inspect_with_devtools);

  ExtensionHost* host() const { return extension_host_.get(); }

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionView::Container overrides.
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view) OVERRIDE;

  // view::View overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // The min/max height of popups.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 private:
  ExtensionPopup(Browser* browser,
                 ExtensionHost* host,
                 views::View* anchor_view,
                 views::BubbleBorder::ArrowLocation arrow_location,
                 bool inspect_with_devtools);

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  // Flag used to indicate if the pop-up should open a devtools window once
  // it is shown inspecting it.
  bool inspect_with_devtools_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
