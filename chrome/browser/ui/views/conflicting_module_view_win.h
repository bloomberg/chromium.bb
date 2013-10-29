// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFLICTING_MODULE_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_CONFLICTING_MODULE_VIEW_WIN_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

class Browser;

namespace views {
class Label;
class LabelButton;
}

// This is the class that implements the UI for the bubble showing that there
// is a 3rd party module loaded that conflicts with Chrome.
class ConflictingModuleView : public views::BubbleDelegateView,
                              public views::ButtonListener,
                              public content::NotificationObserver {
 public:
  ConflictingModuleView(views::View* anchor_view,
                        Browser* browser,
                        const GURL& help_center_url);

  // Show the Disabled Extension bubble, if needed.
  static void MaybeShow(Browser* browser, views::View* anchor_view);

 private:
  virtual ~ConflictingModuleView();

  // Shows the bubble and updates the counter for how often it has been shown.
  void ShowBubble();

  // Dismiss and make sure the bubble is not shown again. This bubble is a
  // single-appearance bubble.
  void DismissBubble();

  // views::BubbleDelegateView implementation:
  virtual void Init() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::View implementation.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) OVERRIDE;

  Browser* browser_;

  content::NotificationRegistrar registrar_;

  // The headline, labels and buttons on the bubble.
  views::Label* explanation_;
  views::LabelButton* learn_more_button_;
  views::LabelButton* not_now_button_;

  // The link to the help center for this conflict.
  GURL help_center_url_;

  DISALLOW_COPY_AND_ASSIGN(ConflictingModuleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFLICTING_MODULE_VIEW_WIN_H_
