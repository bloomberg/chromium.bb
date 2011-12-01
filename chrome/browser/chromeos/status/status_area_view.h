// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
#pragma once

#include <list>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view.h"

// This class is used to wrap the small informative widgets in the upper-right
// of the window title bar. It is used on ChromeOS only.
class StatusAreaView : public views::AccessiblePaneView,
                       public base::SupportsWeakPtr<StatusAreaView> {
 public:
  explicit StatusAreaView();
  virtual ~StatusAreaView();

  void AddButton(StatusAreaButton* button, bool bordered);
  void RemoveButton(StatusAreaButton* button);

  void MakeButtonsActive(bool active);
  void UpdateButtonVisibility();

  // Takes focus and transfers it to the first (last if |reverse| is true).
  // After focus has traversed through all elements, clears focus and calls
  // |return_focus_cb(reverse)| from the message loop.
  typedef base::Callback<void(bool)> ReturnFocusCallback;
  void TakeFocus(bool reverse,
                 const ReturnFocusCallback& return_focus_cb);

  // Overridden from views::FocusChangeListener:
  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) OVERRIDE;

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

 private:
  StatusAreaButton::Delegate* delegate_;

  // True if focus needs to be returned via |return_focus_cb_| when it wraps.
  bool need_return_focus_;
  ReturnFocusCallback return_focus_cb_;

  std::list<StatusAreaButton*> buttons_;

  // Clears focus and calls |return_focus_cb_|.
  void ReturnFocus(bool reverse);

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
