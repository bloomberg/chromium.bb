// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
#pragma once

#include "chrome/browser/views/tab_contents/native_tab_contents_container.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/view.h"

class NativeTabContentsContainer;
class RenderViewHost;
class RenderWidgetHostView;
class TabContents;

class TabContentsContainer : public views::View,
                             public NotificationObserver {
 public:
  // Interface to request the reserved contents area updates.
  class ReservedAreaDelegate {
   public:
    // Notifies that |source|'s reserved contents area should be updated.
    // Reserved contents area is a rect in tab contents view coordinates where
    // contents should not be rendered (to display the resize corner, sidebar
    // mini tabs or any other UI elements overlaying this container).
    virtual void UpdateReservedContentsRect(
        const TabContentsContainer* source) = 0;
   protected:
    virtual ~ReservedAreaDelegate() {}
  };

  TabContentsContainer();
  virtual ~TabContentsContainer();

  // Changes the TabContents associated with this view.
  void ChangeTabContents(TabContents* contents);

  View* GetFocusView() { return native_container_->GetView(); }

  // Accessor for |tab_contents_|.
  TabContents* tab_contents() const { return tab_contents_; }

  // Called by the BrowserView to notify that |tab_contents| got the focus.
  void TabContentsFocused(TabContents* tab_contents);

  // Tells the container to update less frequently during resizing operations
  // so performance is better.
  void SetFastResize(bool fast_resize);

  void set_reserved_area_delegate(ReservedAreaDelegate* delegate) {
    reserved_area_delegate_ = delegate;
  }

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from views::View:
  virtual void Layout();
  virtual AccessibilityTypes::Role GetAccessibleRole();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

 private:
  // Add or remove observers for events that we care about.
  void AddObservers();
  void RemoveObservers();

  // Called when the RenderViewHost of the hosted TabContents has changed, e.g.
  // to show an interstitial page.
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host);

  // Called when a TabContents is destroyed. This gives us a chance to clean
  // up our internal state if the TabContents is somehow destroyed before we
  // get notified.
  void TabContentsDestroyed(TabContents* contents);

  // Called when the RenderWidgetHostView of the hosted TabContents has changed.
  void RenderWidgetHostViewChanged(RenderWidgetHostView* old_view,
                                   RenderWidgetHostView* new_view);

  // An instance of a NativeTabContentsContainer object that holds the native
  // view handle associated with the attached TabContents.
  NativeTabContentsContainer* native_container_;

  // The attached TabContents.
  TabContents* tab_contents_;

  // Handles registering for our notifications.
  NotificationRegistrar registrar_;

  // Delegate for enquiring reserved contents area. Not owned by us.
  ReservedAreaDelegate* reserved_area_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
