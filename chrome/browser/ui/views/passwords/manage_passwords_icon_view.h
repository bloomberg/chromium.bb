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
class ManagePasswordsUIController;

// View for the password icon in the Omnibox.
class ManagePasswordsIconView : public ManagePasswordsIcon,
                                public BubbleIconView {
 public:
  explicit ManagePasswordsIconView(CommandUpdater* updater);
  virtual ~ManagePasswordsIconView();

  // BubbleIconView:
  virtual bool IsBubbleShowing() const OVERRIDE;
  virtual void OnExecuting(BubbleIconView::ExecuteSource source) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // views::View:
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;

#if defined(UNIT_TEST)
  int icon_id() const { return icon_id_; }
  int tooltip_text_id() const { return tooltip_text_id_; }
#endif

 protected:
  // ManagePasswordsIcon:
  virtual void UpdateVisibleUI() OVERRIDE;

 private:

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
