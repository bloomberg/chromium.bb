// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_

#include "ash/common/system/tray/tri_view.h"
#include "base/strings/string16.h"

namespace views {
class ButtonListener;
class ImageView;
class Label;
class LabelButton;
class LayoutManager;
class Slider;
class SliderListener;
}  // namespace views

namespace ash {

// Factory/utility functions used by the system menu.
class TrayPopupUtils {
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

  // Creates the default layout manager by CreateDefault() row for the given
  // |container|. To be used when mutliple targetable areas are required within
  // a single row.
  static std::unique_ptr<views::LayoutManager> CreateLayoutManager(
      TriView::Container container);

  // Returns a label that has been configured for system menu layout. This
  // should be used by all rows that require a label, i.e. both default and
  // detailed rows should use this.
  //
  // TODO(bruthig): Update all system menu rows to use this.
  static views::Label* CreateDefaultLabel();

  // Returns an image view to be used for the main image of a system menu row.
  // This should be used by all rows that have a main image, i.e. both default
  // and detailed rows should use this.
  //
  // TODO(bruthig): Update all system menu rows to use this.
  static views::ImageView* CreateMainImageView();

  // Returns an image view to be used for the 'more' arrow image on rows. In
  // general this applies to all rows in the system menu that have a 'more'
  // image however most, if not all, are default rows.
  //
  // TODO(bruthig): Update all default rows to use this.
  static views::ImageView* CreateMoreImageView();

 private:
  TrayPopupUtils() = delete;
  ~TrayPopupUtils() = delete;

  // Configures the specified |container| view with the default layout. Used by
  // CreateDefaultRowView().
  static void ConfigureDefaultLayout(TriView* tri_view,
                                     TriView::Container container);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_
