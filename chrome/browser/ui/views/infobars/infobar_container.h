// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
#pragma once

#include "chrome/browser/ui/views/accessible_pane_view.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/view.h"

class BrowserView;
class InfoBarDelegate;
class TabContents;

// A views::View subclass that contains a collection of InfoBars associated with
// a TabContents.
class InfoBarContainer : public AccessiblePaneView,
                         public NotificationObserver {
 public:
  // Implement this interface when you want to receive notifications from the
  // InfoBarContainer
  class Delegate {
   public:
    virtual void InfoBarContainerSizeChanged(bool is_animating) = 0;

   protected:
    virtual ~Delegate();
  };

  explicit InfoBarContainer(Delegate* delegate);
  virtual ~InfoBarContainer();

  // Changes the TabContents for which this container is showing InfoBars. Can
  // be NULL.
  void ChangeTabContents(TabContents* contents);

  // Called when a contained infobar has animated.  The container is expected to
  // do anything necessary to respond to the infobar's possible size change,
  // e.g. re-layout.
  void OnInfoBarAnimated(bool done);

  // Remove the specified InfoBarDelegate from the selected TabContents. This
  // will notify us back and cause us to close the View. This is called from
  // the InfoBar's close button handler.
  void RemoveDelegate(InfoBarDelegate* delegate);

  // Paint the InfoBar arrows on |canvas|. |arrow_center_x| indicates
  // the desired location of the center of the arrow in the
  // |outer_view| coordinate system.
  void PaintInfoBarArrows(gfx::Canvas* canvas,
                          View* outer_view,
                          int arrow_center_x);

 private:
  // AccessiblePaneView:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual AccessibilityTypes::Role GetAccessibleRole();

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Constructs the InfoBars needed to reflect the state of the current
  // TabContents associated with this container. No animations are run during
  // this process.
  void UpdateInfoBars();

  // Adds an InfoBar for the specified delegate, in response to a notification
  // from the selected TabContents. The InfoBar's appearance will be animated
  // if |use_animation| is true.
  void AddInfoBar(InfoBarDelegate* delegate, bool use_animation);

  // Removes an InfoBar for the specified delegate, in response to a
  // notification from the selected TabContents. The InfoBar's disappearance
  // will be animated if |use_animation| is true.
  void RemoveInfoBar(InfoBarDelegate* delegate, bool use_animation);

  // Replaces an InfoBar for the specified delegate with a new one. There is no
  // animation.
  void ReplaceInfoBar(InfoBarDelegate* old_delegate,
                      InfoBarDelegate* new_delegate);

  NotificationRegistrar registrar_;
  Delegate* delegate_;
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
