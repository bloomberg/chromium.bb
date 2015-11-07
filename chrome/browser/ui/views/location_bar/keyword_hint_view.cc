// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url_service.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace {

// For MD, the tab key looks just like other omnibox chips.
class TabKeyBubbleView : public IconLabelBubbleView {
 public:
  TabKeyBubbleView(const gfx::FontList& font_list,
                   SkColor text_color,
                   SkColor background_color);
  ~TabKeyBubbleView() override;

 private:
  // IconLabelBubbleView:
  SkColor GetTextColor() const override;
  SkColor GetBorderColor() const override;

  SkColor text_color_;

  DISALLOW_COPY_AND_ASSIGN(TabKeyBubbleView);
};

TabKeyBubbleView::TabKeyBubbleView(const gfx::FontList& font_list,
                                   SkColor text_color,
                                   SkColor background_color)
    : IconLabelBubbleView(0, font_list, SK_ColorRED, background_color, false),
      text_color_(text_color) {
  SetLabel(l10n_util::GetStringUTF16(IDS_APP_TAB_KEY));
}

TabKeyBubbleView::~TabKeyBubbleView() {}

SkColor TabKeyBubbleView::GetTextColor() const {
  return text_color_;
}

SkColor TabKeyBubbleView::GetBorderColor() const {
  return text_color_;
}

}  // namespace

KeywordHintView::KeywordHintView(Profile* profile,
                                 const gfx::FontList& font_list,
                                 const gfx::FontList& bubble_font_list,
                                 int bubble_height,
                                 SkColor text_color,
                                 SkColor background_color)
    : profile_(profile),
      leading_label_(nullptr),
      tab_key_view_(nullptr),
      trailing_label_(nullptr),
      tab_key_height_(bubble_height) {
  leading_label_ =
      CreateLabel(font_list, text_color, background_color);
  if (ui::MaterialDesignController::IsModeMaterial()) {
    tab_key_view_ =
        new TabKeyBubbleView(bubble_font_list, text_color, background_color);
  } else {
    views::ImageView* tab_image = new views::ImageView();
    tab_image->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_OMNIBOX_KEYWORD_HINT_TAB));
    tab_key_view_ = tab_image;
  }
  AddChildView(tab_key_view_);
  trailing_label_ =
      CreateLabel(font_list, text_color, background_color);
}

KeywordHintView::~KeywordHintView() {
}

void KeywordHintView::SetKeyword(const base::string16& keyword) {
  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!url_service)
    return;

  std::vector<size_t> content_param_offsets;
  bool is_extension_keyword;
  base::string16 short_name(
      url_service->GetKeywordShortName(keyword, &is_extension_keyword));
  int message_id = is_extension_keyword ?
      IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
  const base::string16 keyword_hint = l10n_util::GetStringFUTF16(
      message_id, base::string16(), short_name, &content_param_offsets);
  DCHECK_EQ(2U, content_param_offsets.size());
  leading_label_->SetText(
      keyword_hint.substr(0, content_param_offsets.front()));
  trailing_label_->SetText(keyword_hint.substr(content_param_offsets.front()));
}

gfx::Size KeywordHintView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return gfx::Size(leading_label_->GetPreferredSize().width() +
                       tab_key_view_->GetPreferredSize().width() +
                       trailing_label_->GetPreferredSize().width(),
                   0);
}

gfx::Size KeywordHintView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  return tab_key_view_->GetPreferredSize();
}

void KeywordHintView::Layout() {
  int tab_width = tab_key_view_->GetPreferredSize().width();
  bool show_labels = (width() != tab_width);
  gfx::Size leading_size(leading_label_->GetPreferredSize());
  leading_label_->SetBounds(0, 0, show_labels ? leading_size.width() : 0,
                            height());
  tab_key_view_->SetBounds(leading_label_->bounds().right(),
                           (height() - tab_key_height_) / 2, tab_width,
                           tab_key_height_);
  gfx::Size trailing_size(trailing_label_->GetPreferredSize());
  trailing_label_->SetBounds(tab_key_view_->bounds().right(), 0,
                             show_labels ? trailing_size.width() : 0, height());
}

const char* KeywordHintView::GetClassName() const {
  return "KeywordHintView";
}

views::Label* KeywordHintView::CreateLabel(const gfx::FontList& font_list,
                                           SkColor text_color,
                                           SkColor background_color) {
  views::Label* label = new views::Label(base::string16(), font_list);
  label->SetEnabledColor(text_color);
  label->SetBackgroundColor(background_color);
  AddChildView(label);
  return label;
}
