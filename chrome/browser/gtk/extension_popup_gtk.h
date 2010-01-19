// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTENSION_POPUP_GTK_H_
#define CHROME_BROWSER_GTK_EXTENSION_POPUP_GTK_H_

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class ExtensionHost;
class GURL;

class ExtensionPopupGtk : public NotificationObserver,
                          public InfoBubbleGtkDelegate {
 public:
  ExtensionPopupGtk(Browser* browser,
                    ExtensionHost* host,
                    const gfx::Rect& relative_to);
  virtual ~ExtensionPopupGtk();

  static void Show(const GURL& url,
                   Browser* browser,
                   const gfx::Rect& relative_to);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // InfoBubbleGtkDelegate implementation.
  virtual void InfoBubbleClosing(InfoBubbleGtk* bubble,
                                 bool closed_by_escape);

 private:
  // Shows the popup widget. Called after loading completes.
  void ShowPopup();

  // Destroys the popup widget. This will in turn destroy us since we delete
  // ourselves when the info bubble closes. Returns true if we successfully
  // closed the bubble.
  bool DestroyPopup();

  Browser* browser_;

  InfoBubbleGtk* bubble_;

  // We take ownership of the popup ExtensionHost.
  scoped_ptr<ExtensionHost> host_;

  // The rect that we use to position the popup. It is the bounds of the
  // browser action button.
  gfx::Rect relative_to_;

  NotificationRegistrar registrar_;

  // Used for testing. ---------------------------------------------------------
  static ExtensionPopupGtk* get_current_extension_popup() {
    return current_extension_popup_;
  }
  static ExtensionPopupGtk* current_extension_popup_;

  gfx::Rect GetViewBounds();

  friend class BrowserActionTestUtil;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopupGtk);
};

#endif  // CHROME_BROWSER_GTK_EXTENSION_POPUP_GTK_H_
