// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_STATUS_AREA_HOST_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_STATUS_AREA_HOST_AURA_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/status/status_area_button.h"

class StatusAreaView;
class TimezoneClockUpdater;

namespace views {
class Views;
class Widget;
}

class StatusAreaHostAura : public StatusAreaButton::Delegate {
 public:
  StatusAreaHostAura();
  virtual ~StatusAreaHostAura();

  // Returns the view housing the status area. Exposed for testing.
  const views::View* GetStatusArea() const;

  // Instantiates and sets |status_area_view_|, and sets it as the contents of
  // a new views::Widget |status_area_widget_| which is returned.
  // The caller is expected to take ownership of |status_area_widget_|.
  views::Widget* CreateStatusArea();

  // StatusAreaButton::Delegate implementation.
  virtual bool ShouldExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) const OVERRIDE;
  virtual void ExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) OVERRIDE;
  virtual gfx::Font GetStatusAreaFont(const gfx::Font& font) const OVERRIDE;
  virtual StatusAreaButton::TextStyle GetStatusAreaTextStyle() const OVERRIDE;
  virtual void ButtonVisibilityChanged(views::View* button_view) OVERRIDE;

 private:
  // Owned by caller of CreateStatusArea().
  views::Widget* status_area_widget_;
  // Owned by status_area_widget_.
  StatusAreaView* status_area_view_;

  scoped_ptr<TimezoneClockUpdater> timezone_clock_updater_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaHostAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_STATUS_AREA_HOST_AURA_H_
