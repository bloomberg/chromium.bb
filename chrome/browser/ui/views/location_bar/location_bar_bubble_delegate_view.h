// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_BUBBLE_DELEGATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_BUBBLE_DELEGATE_VIEW_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace content {
class NotificationDetails;
class NotificationSource;
class WebContents;
};

// Base class for bubbles that are shown from location bar icons. The bubble
// will automatically close when the browser transitions in or out of fullscreen
// mode.
class LocationBarBubbleDelegateView : public views::BubbleDelegateView,
                                      public content::NotificationObserver {
 public:
  enum DisplayReason {
    // The bubble appears as a direct result of a user action (clicking on the
    // location bar icon).
    USER_GESTURE,

    // The bubble appears spontaneously over the course of the user's
    // interaction with Chrome (e.g. due to some change in the feature's
    // status).
    AUTOMATIC,
  };

  LocationBarBubbleDelegateView(views::View* anchor_view,
                                content::WebContents* web_contents);
  ~LocationBarBubbleDelegateView() override;

  // Displays the bubble with appearance and behavior tailored for |reason|.
  void ShowForReason(DisplayReason reason);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  // Closes the bubble.
  virtual void Close();

  // If the bubble is not anchored to a view, places the bubble in the top right
  // (left in RTL) of the |screen_bounds| that contain web contents's browser
  // window. Because the positioning is based on the size of the bubble, this
  // must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

 private:
  // Used to register for fullscreen change notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarBubbleDelegateView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_BUBBLE_DELEGATE_VIEW_H_
