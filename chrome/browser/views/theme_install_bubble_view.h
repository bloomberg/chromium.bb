// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "views/controls/label.h"
#include "views/widget/widget_win.h"

// ThemeInstallBubbleView launches a "loading..." bubble in the center of the
// currently active browser window when an extension or theme is loaded.  If
// an extension is being applied, the bubble goes away immediately.  If a theme
// is being applied, it disappears when the theme has been loaded.  The purpose
// of this bubble is to warn the user that the browser may be unresponsive
// while the theme is being installed.
//
// Edge case: note that if one installs a theme in one window and then switches
// rapidly to another window to install a theme there as well (in the short
// time between install begin and theme caching seizing the UI thread), the
// loading bubble will only appear over the first window.
class ThemeInstallBubbleView : public NotificationObserver,
                               public views::Label {
 public:
  explicit ThemeInstallBubbleView(TabContents* tab_contents);

  ~ThemeInstallBubbleView();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Show the loading bubble.
  static void Show(TabContents* tab_contents);

 private:
  // The roundedness of the edges of our bubble.
  static const int kBubbleCornerRadius = 4;

  // The content area at the start of the animation.
  gfx::Rect tab_contents_bounds_;

  // We use a HWND for the popup so that it may float above any HWNDs in our UI.
  views::WidgetWin* popup_;

  // Text to show warning that theme is being installed.
  std::wstring text_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  // Put the popup in the correct place on the tab.
  void Reposition();

  // Inherited from views.
  gfx::Size GetPreferredSize();

  // Shut down the popup and remove our notifications.
  void Close();

  virtual void Paint(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(ThemeInstallBubbleView);
};

