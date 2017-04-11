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

#if defined(USE_AURA)
#include "ui/keyboard/keyboard_util.h"
#endif

namespace {

// This view looks somewhat like a keyboard key.
class KeyboardKeyView : public views::Label {
 public:
  explicit KeyboardKeyView(const gfx::FontList& font_list);
  ~KeyboardKeyView() override;

 private:
  // views::Label:
  gfx::Size GetPreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(KeyboardKeyView);
};

bool IsVirtualKeyboardVisible() {
#if defined(USE_AURA)
  return keyboard::IsKeyboardVisible();
#else
  return false;
#endif
}

KeyboardKeyView::KeyboardKeyView(const gfx::FontList& font_list)
    : views::Label(base::string16(), {font_list}) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(LocationBarView::kBubbleVerticalPadding, 0)));
}

KeyboardKeyView::~KeyboardKeyView() {}

gfx::Size KeyboardKeyView::GetPreferredSize() const {
  gfx::Size size = views::Label::GetPreferredSize();
  constexpr int kPaddingInsideBorder = 5;
  // Even though the border is 1 px thick visibly, it takes 1 DIP logically.
  size.Enlarge(2 * (kPaddingInsideBorder + 1), 0);
  return size;
}

}  // namespace

KeywordHintView::KeywordHintView(views::ButtonListener* listener,
                                 Profile* profile,
                                 const gfx::FontList& font_list,
                                 const gfx::FontList& bubble_font_list,
                                 int chip_height,
                                 SkColor text_color,
                                 SkColor background_color)
    : CustomButton(listener),
      profile_(profile),
      leading_label_(nullptr),
      chip_view_(new KeyboardKeyView(bubble_font_list)),
      trailing_label_(nullptr),
      chip_view_height_(chip_height) {
  leading_label_ =
      CreateLabel(font_list, text_color, background_color);
  chip_view_->SetEnabledColor(text_color);
  bool inverted = color_utils::IsDark(background_color);
  SkColor tab_bg_color =
      inverted ? SK_ColorWHITE : SkColorSetA(text_color, 0x13);
  SkColor tab_border_color = inverted ? SK_ColorWHITE : text_color;
  chip_view_->SetBackgroundColor(tab_bg_color);
  chip_view_->set_background(
      new BackgroundWith1PxBorder(tab_bg_color, tab_border_color));
  AddChildView(chip_view_);
  trailing_label_ =
      CreateLabel(font_list, text_color, background_color);
}

KeywordHintView::~KeywordHintView() {}

void KeywordHintView::SetKeyword(const base::string16& keyword) {
  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!url_service)
    return;

  bool is_extension_keyword;
  base::string16 short_name(
      url_service->GetKeywordShortName(keyword, &is_extension_keyword));

  if (IsVirtualKeyboardVisible()) {
    int message_id = is_extension_keyword
                         ? IDS_OMNIBOX_EXTENSION_KEYWORD_HINT_TOUCH
                         : IDS_OMNIBOX_KEYWORD_HINT_TOUCH;
    chip_view_->SetText(l10n_util::GetStringFUTF16(message_id, short_name));

    leading_label_->SetText(base::string16());
    trailing_label_->SetText(base::string16());
  } else {
    chip_view_->SetText(l10n_util::GetStringUTF16(IDS_APP_TAB_KEY));

    std::vector<size_t> content_param_offsets;
    int message_id = is_extension_keyword ? IDS_OMNIBOX_EXTENSION_KEYWORD_HINT
                                          : IDS_OMNIBOX_KEYWORD_HINT;
    const base::string16 keyword_hint = l10n_util::GetStringFUTF16(
        message_id, base::string16(), short_name, &content_param_offsets);
    DCHECK_EQ(2U, content_param_offsets.size());
    leading_label_->SetText(
        keyword_hint.substr(0, content_param_offsets.front()));
    trailing_label_->SetText(
        keyword_hint.substr(content_param_offsets.front()));
  }
}

gfx::Size KeywordHintView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return gfx::Size(leading_label_->GetPreferredSize().width() +
                       chip_view_->GetPreferredSize().width() +
                       trailing_label_->GetPreferredSize().width() +
                       LocationBarView::kIconInteriorPadding,
                   0);
}

gfx::Size KeywordHintView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  return chip_view_->GetPreferredSize();
}

void KeywordHintView::Layout() {
  int chip_width = chip_view_->GetPreferredSize().width();
  bool show_labels = (width() != chip_width);
  gfx::Size leading_size(leading_label_->GetPreferredSize());
  leading_label_->SetBounds(0, 0, show_labels ? leading_size.width() : 0,
                            height());
  chip_view_->SetBounds(leading_label_->bounds().right(),
                        (height() - chip_view_height_) / 2, chip_width,
                        chip_view_height_);
  gfx::Size trailing_size(trailing_label_->GetPreferredSize());
  trailing_label_->SetBounds(chip_view_->bounds().right(), 0,
                             show_labels ? trailing_size.width() : 0, height());
}

const char* KeywordHintView::GetClassName() const {
  return "KeywordHintView";
}

views::Label* KeywordHintView::CreateLabel(const gfx::FontList& font_list,
                                           SkColor text_color,
                                           SkColor background_color) {
  views::Label* label = new views::Label(base::string16(), {font_list});
  label->SetEnabledColor(text_color);
  label->SetBackgroundColor(background_color);
  AddChildView(label);
  return label;
}
