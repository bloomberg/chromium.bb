// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_UTILS_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_UTILS_H_

#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/strings/string16.h"

namespace views {
class Label;
class View;
}

namespace ash {

class TrayItemView;

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

// TODO(jennyz): refactor these two functions to SystemTrayItem.
// Sets the empty border of an image tray item for adjusting the space
// around it.
void SetTrayImageItemBorder(views::View* tray_view, ShelfAlignment alignment);
// Sets the empty border around a label tray item for adjusting the space
// around it.
void SetTrayLabelItemBorder(TrayItemView* tray_view, ShelfAlignment alignment);

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_UTILS_H_
