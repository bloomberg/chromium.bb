// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"

#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/rect.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Equal to the #9F9F9F color used in spec (note WebUI color is #999).
const SkColor kDeemphasizedTextColor = SkColorSetRGB(159, 159, 159);

}  // namespace

MediaGalleryCheckboxView::MediaGalleryCheckboxView(
    const MediaGalleryPrefInfo& pref_info,
    bool show_folder_button,
    int trailing_vertical_space,
    views::ButtonListener* button_listener,
    views::ContextMenuController* menu_controller) {
  DCHECK(button_listener != NULL);
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  SetBorder(views::Border::CreateEmptyBorder(
      0, views::kPanelHorizMargin, trailing_vertical_space,
      views::kPanelHorizMargin));
  if (menu_controller)
    set_context_menu_controller(menu_controller);

  checkbox_ = new views::Checkbox(pref_info.GetGalleryDisplayName());
  checkbox_->set_listener(button_listener);
  if (menu_controller)
    checkbox_->set_context_menu_controller(menu_controller);
  checkbox_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  base::string16 tooltip_text = pref_info.GetGalleryTooltip();
  checkbox_->SetTooltipText(tooltip_text);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  folder_viewer_button_ = new views::ImageButton(button_listener);
  if (menu_controller)
    folder_viewer_button_->set_context_menu_controller(menu_controller);
  folder_viewer_button_->SetImage(views::ImageButton::STATE_NORMAL,
                                  rb.GetImageSkiaNamed(IDR_FILE_FOLDER));
  folder_viewer_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                           views::ImageButton::ALIGN_MIDDLE);
  folder_viewer_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MEDIA_GALLERIES_SCAN_RESULT_OPEN_FOLDER_VIEW_ACCESSIBILITY_NAME));
  folder_viewer_button_->SetFocusable(true);
  folder_viewer_button_->SetVisible(show_folder_button);
  folder_viewer_button_->SetBorder(views::Border::CreateEmptyBorder(
      0, views::kRelatedControlSmallHorizontalSpacing, 0, 0));

  base::string16 details = pref_info.GetGalleryAdditionalDetails();
  secondary_text_ = new views::Label(details);
  if (menu_controller)
    secondary_text_->set_context_menu_controller(menu_controller);
  secondary_text_->SetVisible(details.length() > 0);
  secondary_text_->SetEnabledColor(kDeemphasizedTextColor);
  secondary_text_->SetElideBehavior(gfx::ELIDE_HEAD);
  secondary_text_->SetTooltipText(tooltip_text);
  secondary_text_->SetBorder(views::Border::CreateEmptyBorder(
      0, views::kRelatedControlSmallHorizontalSpacing, 0, 0));

  AddChildView(checkbox_);
  AddChildView(folder_viewer_button_);
  AddChildView(secondary_text_);
}

MediaGalleryCheckboxView::~MediaGalleryCheckboxView() {}

void MediaGalleryCheckboxView::Layout() {
  views::View::Layout();
  if (GetPreferredSize().width() <= GetLocalBounds().width())
    return;

  // If box layout doesn't fit, do custom layout. The folder_viewer_button and
  // the secondary text should take up at most half of the space and the
  // checkbox can take up what ever is left.
  int checkbox_width = checkbox_->GetPreferredSize().width();
  int folder_viewer_width = folder_viewer_button_->GetPreferredSize().width();
  int secondary_text_width = secondary_text_->GetPreferredSize().width();
  if (!folder_viewer_button_->visible())
    folder_viewer_width = 0;
  if (!secondary_text_->visible())
    secondary_text_width = 0;

  gfx::Rect area(GetLocalBounds());
  area.Inset(GetInsets());

  if (folder_viewer_width + secondary_text_width > area.width() / 2) {
    secondary_text_width =
        std::max(area.width() / 2 - folder_viewer_width,
                 area.width() - folder_viewer_width - checkbox_width);
  }
  checkbox_width = area.width() - folder_viewer_width - secondary_text_width;

  checkbox_->SetBounds(area.x(), area.y(), checkbox_width, area.height());
  if (folder_viewer_button_->visible()) {
    folder_viewer_button_->SetBounds(checkbox_->x() + checkbox_width, area.y(),
                                     folder_viewer_width, area.height());
  }
  if (secondary_text_->visible()) {
    secondary_text_->SetBounds(
        checkbox_->x() + checkbox_width + folder_viewer_width,
        area.y(), secondary_text_width, area.height());
  }
}
