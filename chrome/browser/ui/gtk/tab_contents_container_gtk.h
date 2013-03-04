// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CONTAINER_GTK_H_
#define CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CONTAINER_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class StatusBubbleGtk;

namespace content {
class WebContents;
}

typedef struct _GtkFloatingContainer GtkFloatingContainer;

class TabContentsContainerGtk : public content::NotificationObserver,
                                public ViewIDUtil::Delegate {
 public:
  explicit TabContentsContainerGtk(StatusBubbleGtk* status_bubble);
  virtual ~TabContentsContainerGtk();

  void Init();

  // Make the specified tab visible.
  void SetTab(content::WebContents* tab);
  content::WebContents* tab() const { return tab_; }

  void SetOverlay(content::WebContents* overlay);
  bool HasOverlay() const { return overlay_ != NULL; }

  // Returns the WebContents currently displayed.
  content::WebContents* GetVisibleTab() const {
    return overlay_ ? overlay_ : tab_;
  }

  // Remove the tab from the hierarchy.
  void DetachTab(content::WebContents* tab);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  GtkWidget* widget() { return floating_.get(); }

  // ViewIDUtil::Delegate implementation ---------------------------------------
  virtual GtkWidget* GetWidgetForViewID(ViewID id) OVERRIDE;

 private:
  // Called when a WebContents is destroyed. This gives us a chance to clean
  // up our internal state if the WebContents is somehow destroyed before we
  // get notified.
  void WebContentsDestroyed(content::WebContents* contents);

  // Handler for |floating_|'s "set-floating-position" signal. During this
  // callback, we manually set the position of the status bubble.
  static void OnSetFloatingPosition(
      GtkFloatingContainer* container, GtkAllocation* allocation,
      TabContentsContainerGtk* tab_contents_container);

  // Adds |tab| to the container and starts showing it.
  void PackTab(content::WebContents* tab);

  // Stops showing |tab|.
  void HideTab(content::WebContents* tab);

  // Handle focus traversal on the tab contents container. Focus should not
  // traverse to the overlay contents.
  CHROMEGTK_CALLBACK_1(TabContentsContainerGtk, gboolean, OnFocus,
                       GtkDirectionType);

  content::NotificationRegistrar registrar_;

  // The WebContents for the currently selected tab. This will be showing
  // unless there is an overlay contents.
  content::WebContents* tab_;

  // The current overlay contents (for Instant). If non-NULL, it will be
  // visible.
  content::WebContents* overlay_;

  // The status bubble manager.  Always non-NULL.
  StatusBubbleGtk* status_bubble_;

  // Top of the TabContentsContainerGtk widget hierarchy. A cross between a
  // GtkBin and a GtkFixed, |floating_| has |expanded_| as its one "real" child,
  // and the various things that hang off the bottom (status bubble, etc) have
  // their positions manually set in OnSetFloatingPosition.
  ui::OwnedWidgetGtk floating_;

  // We insert and remove WebContents GtkWidgets into this expanded_. This
  // should not be a GtkVBox since there were errors with timing where the vbox
  // was horizontally split with the top half displaying the current WebContents
  // and bottom half displaying the loading page.
  GtkWidget* expanded_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsContainerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CONTAINER_GTK_H_
