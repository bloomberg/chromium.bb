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
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/search_engines/template_url_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"


KeywordHintView::KeywordHintView(Profile* profile,
                                 const gfx::FontList& font_list,
                                 SkColor text_color,
                                 SkColor background_color)
    : profile_(profile),
      tab_image_(new views::ImageView()) {
  leading_label_ =
      CreateLabel(font_list, text_color, background_color);
  tab_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_OMNIBOX_KEYWORD_HINT_TAB));
  AddChildView(tab_image_);
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
                       tab_image_->GetPreferredSize().width() +
                       trailing_label_->GetPreferredSize().width(),
                   0);
}

gfx::Size KeywordHintView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  return tab_image_->GetPreferredSize();
}

void KeywordHintView::Layout() {
  int tab_width = tab_image_->GetPreferredSize().width();
  bool show_labels = (width() != tab_width);
  gfx::Size leading_size(leading_label_->GetPreferredSize());
  leading_label_->SetBounds(0, 0, show_labels ? leading_size.width() : 0,
                            height());
  tab_image_->SetBounds(leading_label_->bounds().right(), 0, tab_width,
                        height());
  gfx::Size trailing_size(trailing_label_->GetPreferredSize());
  trailing_label_->SetBounds(tab_image_->bounds().right(), 0,
                             show_labels ? trailing_size.width() : 0,
                             height());
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
