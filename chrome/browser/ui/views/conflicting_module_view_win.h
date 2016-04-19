// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFLICTING_MODULE_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_CONFLICTING_MODULE_VIEW_WIN_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "url/gurl.h"

class Browser;

// This is the class that implements the UI for the bubble showing that there
// is a 3rd party module loaded that conflicts with Chrome.
class ConflictingModuleView : public views::BubbleDialogDelegateView,
                              public content::NotificationObserver {
 public:
  ConflictingModuleView(views::View* anchor_view,
                        Browser* browser,
                        const GURL& help_center_url);

  // Show the Disabled Extension bubble, if needed.
  static void MaybeShow(Browser* browser, views::View* anchor_view);

 private:
  ~ConflictingModuleView() override;

  // Shows the bubble and updates the counter for how often it has been shown.
  void ShowBubble();

  // views::BubbleDialogDelegateView implementation:
  void OnWidgetClosing(views::Widget* widget) override;
  bool Accept() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void Init() override;

  // views::View implementation.
  void GetAccessibleState(ui::AXViewState* state) override;

  // content::NotificationObserver implementation.
  void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) override;

  Browser* browser_;

  content::NotificationRegistrar registrar_;

  // The link to the help center for this conflict.
  GURL help_center_url_;

  DISALLOW_COPY_AND_ASSIGN(ConflictingModuleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFLICTING_MODULE_VIEW_WIN_H_
