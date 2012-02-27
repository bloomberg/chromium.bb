// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/views/view.h"

#include <vector>

namespace views {
class View;
}

namespace ash {

class SystemTrayItem;

class ASH_EXPORT SystemTray : public views::View {
 public:
  SystemTray();
  virtual ~SystemTray();

  // Adds a new item in the tray.
  void AddTrayItem(SystemTrayItem* item);

  // Removes an existing tray item.
  void RemoveTrayItem(SystemTrayItem* item);

  const std::vector<SystemTrayItem*>& items() const { return items_; }

 private:
  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  std::vector<SystemTrayItem*> items_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
