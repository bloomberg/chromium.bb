// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_CONTAINER_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_CONTAINER_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class InfoBar;
class InfoBarDelegate;
class Profile;
class TabContents;

typedef struct _GtkWidget GtkWidget;

class InfoBarContainerGtk : public NotificationObserver {
 public:
  explicit InfoBarContainerGtk(Profile* profile);
  virtual ~InfoBarContainerGtk();

  // Get the native widget.
  GtkWidget* widget() const { return container_.get(); }

  // Changes the TabContents for which this container is showing InfoBars. Can
  // be NULL, in which case we will simply detach ourselves from the old tab
  // contents.
  void ChangeTabContents(TabContents* contents);

  // Remove the specified InfoBarDelegate from the selected TabContents. This
  // will notify us back and cause us to close the View. This is called from
  // the InfoBar's close button handler.
  void RemoveDelegate(InfoBarDelegate* delegate);

  // Returns the total pixel height of all infobars in this container that
  // are currently animating.
  int TotalHeightOfAnimatingBars() const;

 private:
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Constructs the InfoBars needed to reflect the state of the current
  // TabContents associated with this container. No animations are run during
  // this process.
  void UpdateInfoBars();

  // Makes the calls to show an arrow for |delegate| (either on the browser
  // toolbar or on the next infobar up).
  void ShowArrowForDelegate(InfoBarDelegate* delegate, bool animate);

  // Adds an InfoBar for the specified delegate, in response to a notification
  // from the selected TabContents.
  void AddInfoBar(InfoBarDelegate* delegate, bool animate);

  // Removes an InfoBar for the specified delegate, in response to a
  // notification from the selected TabContents. The InfoBar's disappearance
  // will be animated.
  void RemoveInfoBar(InfoBarDelegate* delegate, bool animate);

  // Tells the browser window about our state so it can draw the arrow
  // appropriately.
  void UpdateToolbarInfoBarState(InfoBar* infobar, bool animate);

  NotificationRegistrar registrar_;

  // The profile for the browser that hosts this InfoBarContainer.
  Profile* profile_;

  // The TabContents for which we are currently showing InfoBars.
  TabContents* tab_contents_;

  // VBox that holds the info bars.
  OwnedWidgetGtk container_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_CONTAINER_GTK_H_
