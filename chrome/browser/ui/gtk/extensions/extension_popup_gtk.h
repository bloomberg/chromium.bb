// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_POPUP_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_POPUP_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

class Browser;
class ExtensionHost;
class GURL;

class ExtensionPopupGtk : public content::NotificationObserver,
                          public BubbleDelegateGtk,
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

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble,
                             bool closed_by_escape) OVERRIDE;

  // ExtensionViewGtk::Container implementation.
  virtual void OnExtensionPreferredSizeChanged(
      ExtensionViewGtk* view,
      const gfx::Size& new_size) OVERRIDE;

  // Destroys the popup widget. This will in turn destroy us since we delete
  // ourselves when the bubble closes. Returns true if we successfully
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

  // See DestroyPopup. Does not return success or failure. Necessitated by
  // base::Bind and friends, which cannot handle a WeakPtr for a function that
  // has a return value.
  void DestroyPopupWithoutResult();

  Browser* browser_;

  BubbleGtk* bubble_;

  // We take ownership of the popup ExtensionHost.
  scoped_ptr<ExtensionHost> host_;

  // The widget for anchoring the position of the bubble.
  GtkWidget* anchor_;

  content::NotificationRegistrar registrar_;

  static ExtensionPopupGtk* current_extension_popup_;

  // Whether a devtools window is attached to this bubble.
  bool being_inspected_;

  base::WeakPtrFactory<ExtensionPopupGtk> weak_factory_;

  // Used for testing. ---------------------------------------------------------
  gfx::Rect GetViewBounds();

  friend class BrowserActionTestUtil;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopupGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_POPUP_GTK_H_
