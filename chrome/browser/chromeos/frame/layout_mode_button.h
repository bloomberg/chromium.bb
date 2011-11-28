// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_LAYOUT_MODE_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_LAYOUT_MODE_BUTTON_H_
#pragma once

#include "base/basictypes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace chromeos {

// Maximize/restore button in the corner of each browser window.
class LayoutModeButton : public views::ImageButton,
                         public views::ButtonListener,
                         public content::NotificationObserver {
 public:
  LayoutModeButton();
  virtual ~LayoutModeButton();

  // views::* override.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void Init();

 private:
  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event)
      OVERRIDE;

  // Update our icon and tooltip appropriately for the current layout mode.
  void UpdateForCurrentLayoutMode();

  // A registrar for subscribing to LAYOUT_MODE_CHANGED notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LayoutModeButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_LAYOUT_MODE_BUTTON_H_
