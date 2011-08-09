// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_MEMORY_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_MEMORY_MENU_BUTTON_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace views {
class MenuItemView;
}

namespace chromeos {

class StatusAreaHost;

// Memory debugging display that lives in the status area.
class MemoryMenuButton : public StatusAreaButton,
                         public views::MenuDelegate,
                         public views::ViewMenuDelegate {
 public:
  explicit MemoryMenuButton(StatusAreaHost* host);
  virtual ~MemoryMenuButton();

  // views::MenuDelegate implementation
  virtual std::wstring GetLabel(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

  // Updates the text on the menu button.
  void UpdateText();

 protected:
  virtual int horizontal_padding() OVERRIDE;

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Execute command id for each renderer. Used for heap profiling.
  void SendCommandToRenderers(int id);

  // Create and initialize menu if not already present.
  void EnsureMenu();

  // Updates text and schedules the timer to fire at the next minute interval.
  void UpdateTextAndSetNextTimer();

  base::OneShotTimer<MemoryMenuButton> timer_;

  // NOTE: we use a scoped_ptr here as menu calls into 'this' from the
  // constructor.
  scoped_ptr<views::MenuItemView> menu_;

  int mem_total_;
  int shmem_;  // video driver memory, hidden from OS
  int mem_free_;
  int mem_buffers_;
  int mem_cache_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_MEMORY_MENU_BUTTON_H_
