// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LAYOUT_FACTORY_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LAYOUT_FACTORY_H_

#include <memory>

#include "ash/common/system/tray/tri_view.h"
#include "base/macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Factory functions for system menu row container views.
class TrayPopupLayoutFactory {
 public:
  // Creates a default container view to be used most system menu rows. The
  // caller takes over ownership of the created view.
  //
  // The returned view consists of 3 regions: START, CENTER, and END. Any child
  // Views added to the START and END containers will be added horizonatlly and
  // any Views added to the CENTER container will be added vertically.
  //
  // The START and END containers have a fixed width but can grow into the
  // CENTER container if space is required and available.
  //
  // The CENTER container has a flexible width.
  static TriView* CreateDefaultRowView();

 private:
  TrayPopupLayoutFactory();
  ~TrayPopupLayoutFactory();

  // Configures the specified |container| view with the default layout. Used by
  // CreateDefaultRowView().
  static void ConfigureDefaultLayout(TriView* tri_view,
                                     TriView::Container container);

  DISALLOW_COPY_AND_ASSIGN(TrayPopupLayoutFactory);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_LAYOUT_FACTORY_H_
