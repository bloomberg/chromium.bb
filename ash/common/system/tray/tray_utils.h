// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_UTILS_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_UTILS_H_

#include <vector>

#include "ash/common/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/strings/string16.h"

namespace views {
class Label;
class Separator;
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

// Computes an accessible label for this button based on all descendant view
// labels by concatenating them in depth-first order.
void GetAccessibleLabelFromDescendantViews(
    views::View* view,
    std::vector<base::string16>& out_labels);

// Returns true if it is possible to open WebUI settings in a browser window,
// i.e., the user is logged in, not on the lock screen, and not in a secondary
// account flow.
bool CanOpenWebUISettings(LoginStatus status);

// Creates and returns a vertical separator to be used between two items in a
// material design system menu row. The caller assumes ownership of the
// returned separator.
views::Separator* CreateVerticalSeparator();

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_UTILS_H_
