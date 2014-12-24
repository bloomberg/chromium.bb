// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MANAGED_FULL_SCREEN_BUBBLE_DELEGATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MANAGED_FULL_SCREEN_BUBBLE_DELEGATE_VIEW_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace content {
class NotificationDetails;
class NotificationSource;
class WebContents;
};

// View used to automatically close a bubble when the browser transitions in or
// out of fullscreen mode.
class ManagedFullScreenBubbleDelegateView
    : public views::BubbleDelegateView,
      public content::NotificationObserver {
 public:
  ManagedFullScreenBubbleDelegateView(views::View* anchor_view,
                                      content::WebContents* web_contents);
  ~ManagedFullScreenBubbleDelegateView() override;

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

  DISALLOW_COPY_AND_ASSIGN(ManagedFullScreenBubbleDelegateView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_MANAGED_FULL_SCREEN_BUBBLE_DELEGATE_VIEW_H_
