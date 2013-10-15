// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CONTAINER_GTK_H_
#define CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CONTAINER_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class StatusBubbleGtk;

typedef struct _GtkFloatingContainer GtkFloatingContainer;

class TabContentsContainerGtk : protected content::WebContentsObserver,
                                public ViewIDUtil::Delegate {
 public:
  TabContentsContainerGtk(StatusBubbleGtk* status_bubble,
                          bool embed_fullscreen_widget);
  virtual ~TabContentsContainerGtk();

  // Make the specified tab visible.
  void SetTab(content::WebContents* tab);
  content::WebContents* tab() const { return web_contents(); }

  // Remove the tab from the hierarchy.
  void DetachTab(content::WebContents* tab);

  GtkWidget* widget() { return floating_.get(); }

  // ViewIDUtil::Delegate implementation ---------------------------------------
  virtual GtkWidget* GetWidgetForViewID(ViewID id) OVERRIDE;

 private:
  // Overridden from content::WebContentsObserver:
  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE;
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE;
  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE;

  // Handler for |floating_|'s "set-floating-position" signal. During this
  // callback, we manually set the position of the status bubble.
  static void OnSetFloatingPosition(
      GtkFloatingContainer* container, GtkAllocation* allocation,
      TabContentsContainerGtk* tab_contents_container);

  // Helper to add the WebContents view (or fullscreen view) to |expanded_|.
  void PackTab();

  // Helper to hide the WebContents view (or fullscreen view) in |expanded_|.
  void HideTab();

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

  // When true, TabContentsContainerGtk auto-embeds fullscreen widgets as a
  // child view in response to DidShow/DidDestroyFullscreenWidget events.
  bool should_embed_fullscreen_widgets_;

  // Set to true while TabContentsContainerGtk is embedding a fullscreen widget
  // view in |expanded_|, with the normal WebContentsView render view hidden.
  bool is_embedding_fullscreen_widget_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsContainerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_TAB_CONTENTS_CONTAINER_GTK_H_
