// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

class CommandUpdater;
class ManagePasswordsBubbleUIController;

// View for the password icon in the Omnibox.
class ManagePasswordsIconView : public ManagePasswordsIcon,
                                public BubbleIconView {
 public:
  // Clicking on the ManagePasswordsIconView shows a ManagePasswordsBubbleView,
  // which requires the current WebContents. Because the current WebContents
  // changes as the user switches tabs, it cannot be provided in the
  // constructor. Instead, a LocationBarView::Delegate is passed here so that it
  // can be queried for the current WebContents as needed.
  ManagePasswordsIconView(LocationBarView::Delegate* location_bar_delegate,
                          CommandUpdater* command_updater);
  virtual ~ManagePasswordsIconView();

  // ManagePasswordsIcon:
  //
  // TODO(mkwst): Remove this once we get rid of the single call to
  // ShowBubbleWithoutUserInteraction in ManagePasswordsBubbleUIController.
  virtual void ShowBubbleWithoutUserInteraction() OVERRIDE;

  // BubbleIconView:
  virtual bool IsBubbleShowing() const OVERRIDE;
  virtual void OnExecuting(BubbleIconView::ExecuteSource source) OVERRIDE;

#if defined(UNIT_TEST)
  int icon_id() const { return icon_id_; }
  int tooltip_text_id() const { return tooltip_text_id_; }
#endif

 protected:
  // ManagePasswordsIcon:
  virtual void UpdateVisibleUI() OVERRIDE;

 private:
  // The delegate used to get the currently visible WebContents.
  LocationBarView::Delegate* location_bar_delegate_;

  // The updater used to deliver commands to the browser; we'll use this to
  // pop open the bubble when necessary.
  //
  // TODO(mkwst): Remove this once we get rid of the single call to
  // ShowBubbleWithoutUserInteraction in ManagePasswordsBubbleUIController.
  CommandUpdater* command_updater_;

  // The ID of the icon and text resources that are currently displayed.
  int icon_id_;
  int tooltip_text_id_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
