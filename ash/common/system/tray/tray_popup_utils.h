// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_

#include "ash/common/login_status.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_ink_drop_style.h"
#include "ash/common/system/tray/tri_view.h"
#include "base/strings/string16.h"

namespace views {
class ButtonListener;
class ImageView;
class InkDrop;
class InkDropRipple;
class InkDropHighlight;
class InkDropHostView;
class InkDropMask;
class Label;
class LabelButton;
class Separator;
class Slider;
class SliderListener;
class ToggleButton;
}  // namespace views

namespace ash {

// Factory/utility functions used by the system menu.
class TrayPopupUtils {
 public:
  // Creates a default container view to be used by system menu rows that are
  // either a single targetable area or not targetable at all. The caller takes
  // over ownership of the created view.
  //
  // The returned view consists of 3 regions: START, CENTER, and END. Any child
  // Views added to the START and END containers will be added horizontally and
  // any Views added to the CENTER container will be added vertically.
  //
  // The START and END containers have a fixed minimum width but can grow into
  // the CENTER container if space is required and available.
  //
  // The CENTER container has a flexible width.
  static TriView* CreateDefaultRowView();

  // Creates a container view to be used by system menu rows that want to embed
  // a targetable area within one (or more) of the containers OR by any row
  // that requires a non-default layout within the container views. The returned
  // view will have the following configurations:
  //   - default minimum row height
  //   - default minimum width for the START and END containers
  //   - default left and right insets
  //   - default container flex values
  //   - Each container view will have a FillLayout installed on it
  //
  // The caller takes over ownership of the created view.
  //
  // The START and END containers have a fixed minimum width but can grow into
  // the CENTER container if space is required and available. The CENTER
  // container has a flexible width.
  //
  // Clients can use ConfigureContainer() to configure their own container views
  // before adding them to the returned TriView.
  static TriView* CreateMultiTargetRowView();

  // Returns a label that has been configured for system menu layout. This
  // should be used by all rows that require a label, i.e. both default and
  // detailed rows should use this.
  //
  // TODO(bruthig): Update all system menu rows to use this.
  static views::Label* CreateDefaultLabel();

  // Returns an image view to be used in the main image region of a system menu
  // row. This should be used by all rows that have a main image, i.e. both
  // default and detailed rows should use this.
  //
  // TODO(bruthig): Update all system menu rows to use this.
  static views::ImageView* CreateMainImageView();

  // Returns an image view to be used in the 'more' region of default rows. This
  // is used for all 'more' images as well as other images that appear in this
  // region, e.g. audio output icon.
  //
  // TODO(bruthig): Update all default rows to use this.
  static views::ImageView* CreateMoreImageView();

  // Returns a slider configured for proper layout within a TriView container
  // with a FillLayout.
  static views::Slider* CreateSlider(views::SliderListener* listener);

  // Returns a ToggleButton that has been configured for system menu layout.
  static views::ToggleButton* CreateToggleButton(
      views::ButtonListener* listener,
      int accessible_name_id);

  // Sets up |view| to be a sticky header in a tray detail scroll view.
  static void ConfigureAsStickyHeader(views::View* view);

  // Configures |container_view| just like CreateDefaultRowView() would
  // configure |container| on its returned TriView. To be used when mutliple
  // targetable areas are required within a single row.
  static void ConfigureContainer(TriView::Container container,
                                 views::View* container_view);

  // Creates a button for use in the system menu that only has a visible border
  // when being hovered/clicked. Caller assumes ownership.
  static views::LabelButton* CreateTrayPopupBorderlessButton(
      views::ButtonListener* listener,
      const base::string16& text);

  // Creates a button for use in the system menu. For MD, this is a prominent
  // text
  // button. For non-MD, this does the same thing as the above. Caller assumes
  // ownership.
  static views::LabelButton* CreateTrayPopupButton(
      views::ButtonListener* listener,
      const base::string16& text);

  // Creates and returns a vertical separator to be used between two items in a
  // material design system menu row. The caller assumes ownership of the
  // returned separator.
  static views::Separator* CreateVerticalSeparator();

  // Creates in InkDrop instance for |host| according to the |ink_drop_style|.
  // All styles are configured to show the highlight when the ripple is visible.
  //
  // All targetable views in the system menu should delegate
  // InkDropHost::CreateInkDrop() calls here.
  static std::unique_ptr<views::InkDrop> CreateInkDrop(
      TrayPopupInkDropStyle ink_drop_style,
      views::InkDropHostView* host);

  // Creates an InkDropRipple instance for |host| according to the
  // |ink_drop_style|. The ripple will be centered on |center_point|.
  //
  // All targetable views in the system menu should delegate
  // InkDropHost::CreateInkDropRipple() calls here.
  static std::unique_ptr<views::InkDropRipple> CreateInkDropRipple(
      TrayPopupInkDropStyle ink_drop_style,
      const views::View* host,
      const gfx::Point& center_point,
      SkColor color = kTrayPopupInkDropBaseColor);

  // Creates in InkDropHighlight instance for |host| according to the
  // |ink_drop_style|.
  //
  // All targetable views in the system menu should delegate
  // InkDropHost::CreateInkDropHighlight() calls here.
  static std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
      TrayPopupInkDropStyle ink_drop_style,
      const views::View* host,
      SkColor color = kTrayPopupInkDropBaseColor);

  // Creates in InkDropMask instance for |host| according to the
  // |ink_drop_style|. May return null.
  //
  // All targetable views in the system menu should delegate
  // InkDropHost::CreateInkDropMask() calls here.
  static std::unique_ptr<views::InkDropMask> CreateInkDropMask(
      TrayPopupInkDropStyle ink_drop_style,
      const views::View* host);

  // Creates and returns a horizontal separator line to be drawn between rows
  // in a detailed view. If |left_inset| is true, then the separator is inset on
  // the left by the width normally occupied by an icon. Caller assumes
  // ownership of the returned separator.
  static views::Separator* CreateListItemSeparator(bool left_inset);

  // Returns true if it is possible to open WebUI settings in a browser window,
  // i.e., the user is logged in, not on the lock screen, and not in a secondary
  // account flow.
  static bool CanOpenWebUISettings(LoginStatus status);

 private:
  // Returns the effective ink drop insets for |host| according to the
  // |ink_drop_style|.
  static gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style);

  // Returns the effective ink drop bounds for |host| according to the
  // |ink_drop_style|.
  static gfx::Rect GetInkDropBounds(TrayPopupInkDropStyle ink_drop_style,
                                    const views::View* host);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TrayPopupUtils);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_
