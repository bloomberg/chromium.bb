// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class InfoBar;
class InfoBarDelegate;
class TabContents;

// InfoBarContainer is a cross-platform base class to handle the visibility-
// related aspects of InfoBars.  While InfoBars own themselves, the
// InfoBarContainer is responsible for telling particular InfoBars that they
// should be hidden or visible.
//
// Platforms need to subclass this to implement a few platform-specific
// functions, which are pure virtual here.
class InfoBarContainer : public NotificationObserver {
 public:
  class Delegate {
   public:
    // The delegate is notified each time the infobar container changes height.
    virtual void InfoBarContainerHeightChanged(bool is_animating) = 0;

    // The delegate needs to tell us whether "unspoofable" arrows should be
    // drawn, and if so, at what |x| coordinate.  |x| may not be NULL.
    virtual bool DrawInfoBarArrows(int* x) const = 0;

   protected:
    virtual ~Delegate();
  };

  explicit InfoBarContainer(Delegate* delegate);
  virtual ~InfoBarContainer();

  // Changes the TabContents for which this container is showing infobars.  This
  // will remove all current infobars from the container, add the infobars from
  // |contents|, and show them all.  |contents| may be NULL.
  void ChangeTabContents(TabContents* contents);

  // Return the amount by which to overlap the toolbar above, and, when
  // |total_height| is non-NULL, set it to the height of the InfoBarContainer
  // (including overlap).
  int GetVerticalOverlap(int* total_height);

  // Called when a contained infobar has animated or by some other means changed
  // its height.  The container is expected to do anything necessary to respond,
  // e.g. re-layout.
  void OnInfoBarHeightChanged(bool is_animating);

  // Passthrough to the delegate function of the same name.
  bool DrawInfoBarArrows(int* x) const;

  // Remove the specified InfoBarDelegate from the selected TabContents. This
  // will notify us back and cause us to close the InfoBar.  This is called from
  // the InfoBar's close button handler.
  void RemoveDelegate(InfoBarDelegate* delegate);

  // Called by |infobar| to request that it be removed from the container, as it
  // is about to delete itself.  At this point, |infobar| should already be
  // hidden.
  void RemoveInfoBar(InfoBar* infobar);

 protected:
  // Subclasses must call this during destruction, so that we can remove
  // infobars (which will call the pure virtual functions below) while the
  // subclass portion of |this| has not yet been destroyed.
  void RemoveAllInfoBarsForDestruction();

  // These must be implemented on each platform to e.g. adjust the visible
  // object hierarchy.
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar) = 0;
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) = 0;

 private:
  typedef std::vector<InfoBar*> InfoBars;

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Removes an InfoBar for the specified delegate, in response to a
  // notification from the selected TabContents. The InfoBar's disappearance
  // will be animated if |use_animation| is true.
  void RemoveInfoBar(InfoBarDelegate* delegate, bool use_animation);

  // Adds |infobar| to this container and calls Show() on it.  |animate| is
  // passed along to infobar->Show().  Depending on the value of
  // |callback_status|, this calls infobar->set_container(this) either before or
  // after the call to Show() so that OnInfoBarAnimated() either will or won't
  // be called as a result.
  enum CallbackStatus { NO_CALLBACK, WANT_CALLBACK };
  void AddInfoBar(InfoBar* infobar,
                  bool animate,
                  CallbackStatus callback_status);

  NotificationRegistrar registrar_;
  Delegate* delegate_;
  TabContents* tab_contents_;
  InfoBars infobars_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_H_
