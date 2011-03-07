// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSION_POPUP_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSION_POPUP_GTK_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/ui/gtk/extension_view_gtk.h"
#include "chrome/browser/ui/gtk/info_bubble_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/rect.h"

class Browser;
class ExtensionHost;
class GURL;

class ExtensionPopupGtk : public NotificationObserver,
                          public InfoBubbleGtkDelegate,
                          public ExtensionViewGtk::Container {
 public:
  ExtensionPopupGtk(Browser* browser,
                    ExtensionHost* host,
                    GtkWidget* anchor,
                    bool inspect);
  virtual ~ExtensionPopupGtk();

  static void Show(const GURL& url,
                   Browser* browser,
                   GtkWidget* anchor,
                   bool inspect);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // InfoBubbleGtkDelegate implementation.
  virtual void InfoBubbleClosing(InfoBubbleGtk* bubble,
                                 bool closed_by_escape);

  // ExtensionViewGtk::Container implementation
  virtual void OnExtensionPreferredSizeChanged(ExtensionViewGtk* view,
                                               const gfx::Size& new_size);

  // Destroys the popup widget. This will in turn destroy us since we delete
  // ourselves when the info bubble closes. Returns true if we successfully
  // closed the bubble.
  bool DestroyPopup();

  // Get the currently showing extension popup, or NULL.
  static ExtensionPopupGtk* get_current_extension_popup() {
    return current_extension_popup_;
  }

  bool being_inspected() const {
    return being_inspected_;
  }

  // Declared here for testing.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 private:
  // Shows the popup widget. Called after loading completes.
  void ShowPopup();

  Browser* browser_;

  InfoBubbleGtk* bubble_;

  // We take ownership of the popup ExtensionHost.
  scoped_ptr<ExtensionHost> host_;

  // The widget for anchoring the position of the info bubble.
  GtkWidget* anchor_;

  NotificationRegistrar registrar_;

  static ExtensionPopupGtk* current_extension_popup_;

  // Whether a devtools window is attached to this bubble.
  bool being_inspected_;

  ScopedRunnableMethodFactory<ExtensionPopupGtk> method_factory_;

  // Used for testing. ---------------------------------------------------------
  gfx::Rect GetViewBounds();

  friend class BrowserActionTestUtil;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopupGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSION_POPUP_GTK_H_
