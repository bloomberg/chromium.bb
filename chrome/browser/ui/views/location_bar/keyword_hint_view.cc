// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace {

// This view sort of looks like a tab key.
class TabKeyBubbleView : public views::Label {
 public:
  explicit TabKeyBubbleView(const gfx::FontList& font_list);
  ~TabKeyBubbleView() override;

 private:
  // views::Label:
  gfx::Size GetPreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(TabKeyBubbleView);
};

TabKeyBubbleView::TabKeyBubbleView(const gfx::FontList& font_list)
    : views::Label(l10n_util::GetStringUTF16(IDS_APP_TAB_KEY), font_list) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(LocationBarView::kBubbleVerticalPadding, 0)));
}

TabKeyBubbleView::~TabKeyBubbleView() {}

gfx::Size TabKeyBubbleView::GetPreferredSize() const {
  gfx::Size size = views::Label::GetPreferredSize();
  constexpr int kPaddingInsideBorder = 5;
  // Even though the border is 1 px thick visibly, it takes 1 DIP logically.
  size.Enlarge(2 * (kPaddingInsideBorder + 1), 0);
  return size;
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
  TabKeyBubbleView* tab_key = new TabKeyBubbleView(bubble_font_list);
  tab_key->SetEnabledColor(text_color);
  bool inverted = color_utils::IsDark(background_color);
  SkColor tab_bg_color =
      inverted ? SK_ColorWHITE : SkColorSetA(text_color, 0x13);
  SkColor tab_border_color = inverted ? SK_ColorWHITE : text_color;
  tab_key->SetBackgroundColor(tab_bg_color);
  tab_key->set_background(
      new BackgroundWith1PxBorder(tab_bg_color, tab_border_color));
  tab_key_view_ = tab_key;
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
                       trailing_label_->GetPreferredSize().width() +
                       LocationBarView::kIconInteriorPadding,
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
