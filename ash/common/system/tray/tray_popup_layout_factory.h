// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LAYOUT_FACTORY_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LAYOUT_FACTORY_H_

#include <memory>

#include "ash/common/system/tray/three_view_layout.h"
#include "base/macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Factory functions for system menu row layout managers.
class TrayPopupLayoutFactory {
 public:
  // Sets the layout manager of |host| to the default layout manager used by
  // most system menu rows. The newly created layout manager is owned by the
  // |host| but is returned so child Views can be added.
  //
  // The returned layout manager consists of 3 regions: START, CENTER, and END.
  // Any child Views added to the START and END containers will be added
  // horizonatlly and any Views added to the CENTER container will be added
  // vertically.
  //
  // The START and END containers have a fixed width but can grow into the
  // CENTER container if space is required and available.
  //
  // The CENTER container has a flexible width.
  static ThreeViewLayout* InstallDefaultLayout(views::View* host);

  // Configures the different |container| Views with the default layout
  // managers.
  //
  // This can be used by clients who need to embed portions of the default
  // layout manager into a different container view, e.g. just the END layout
  // into a clickable View.
  static void ConfigureDefaultLayout(ThreeViewLayout* layout,
                                     ThreeViewLayout::Container container);

 private:
  TrayPopupLayoutFactory();
  ~TrayPopupLayoutFactory();

  DISALLOW_COPY_AND_ASSIGN(TrayPopupLayoutFactory);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LAYOUT_FACTORY_H_
