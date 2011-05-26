// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/views/browser_bubble.h"
#include "chrome/browser/ui/views/extensions/extension_view.h"
#include "content/common/notification_observer.h"

class Browser;
class ExtensionHost;
class GURL;
class Profile;

namespace views {
class Widget;
}

// Modal dialog containing contents provided by an extension.
// Dialog is automatically centered in the browser window and has fixed size.
// For example, used by the Chrome OS file browser.
// TODO(jamescook):  This feels odd as a BrowserBubble.  Factor out a common
// base class for ExtensionDialog and BrowserBubble.
class ExtensionDialog : public BrowserBubble,
                        public BrowserBubble::Delegate,
                        public ExtensionView::Container,
                        public NotificationObserver,
                        public base::RefCounted<ExtensionDialog> {
 public:
  // Observer to ExtensionDialog events.
  class Observer {
   public:
    // Called when the ExtensionDialog is closing. Note that it
    // is ref-counted, and thus will be released shortly after
    // making this delegate call.
    virtual void ExtensionDialogIsClosing(ExtensionDialog* popup) = 0;
  };

  virtual ~ExtensionDialog();

  // Create and show a dialog with |url| centered over the browser window.
  // |browser| is the browser to which the pop-up will be attached.
  // |width| and |height| are the size of the dialog in pixels.
  static ExtensionDialog* Show(const GURL& url, Browser* browser,
                               int width,
                               int height,
                               Observer* observer);

  // Closes the ExtensionDialog.
  void Close();

  ExtensionHost* host() const { return extension_host_.get(); }

  // BrowserBubble overrides.
  virtual void Show(bool activate);

  // BrowserBubble::Delegate methods.
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble);
  virtual void BubbleGotFocus(BrowserBubble* bubble);
  virtual void BubbleLostFocus(BrowserBubble* bubble,
                               bool lost_focus_to_child);

  // ExtensionView::Container overrides.
  virtual void OnExtensionMouseMove(ExtensionView* view);
  virtual void OnExtensionMouseLeave(ExtensionView* view);
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view);

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  ExtensionDialog(ExtensionHost* host, views::Widget* frame,
                  const gfx::Rect& relative_to, int width, int height,
                  Observer* observer);

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  // Dialog size in pixels.
  int width_;
  int height_;

  // Whether the ExtensionDialog is current going about closing itself.
  bool closing_;

  NotificationRegistrar registrar_;

  // The observer of this popup.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
