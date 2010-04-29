// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar/selected_keyword_view.h"

#include "app/l10n_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/location_bar/keyword_hint_view.h"
#include "grit/generated_resources.h"

SelectedKeywordView::SelectedKeywordView(const int background_images[],
                                         int contained_image,
                                         const SkColor& color,
                                         Profile* profile)
    : IconLabelBubbleView(background_images, contained_image, color),
      profile_(profile) {
  full_label_.SetVisible(false);
  partial_label_.SetVisible(false);
}

SelectedKeywordView::~SelectedKeywordView() {
}

void SelectedKeywordView::SetFont(const gfx::Font& font) {
  IconLabelBubbleView::SetFont(font);
  full_label_.SetFont(font);
  partial_label_.SetFont(font);
}

gfx::Size SelectedKeywordView::GetPreferredSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(full_label_.GetPreferredSize().width(), 0);
  return size;
}

gfx::Size SelectedKeywordView::GetMinimumSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(partial_label_.GetMinimumSize().width(), 0);
  return size;
}

void SelectedKeywordView::Layout() {
  SetLabel((width() == GetPreferredSize().width()) ?
      full_label_.GetText() : partial_label_.GetText());
  IconLabelBubbleView::Layout();
}

void SelectedKeywordView::SetKeyword(const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring short_name =
      KeywordHintView::GetKeywordName(profile_, keyword);
  full_label_.SetText(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                            short_name));
  const std::wstring min_string = CalculateMinString(short_name);
  partial_label_.SetText(min_string.empty() ?
      full_label_.GetText() :
      l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT, min_string));
}

std::wstring SelectedKeywordView::CalculateMinString(
    const std::wstring& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find(L'.');
  const size_t ws_index = description.find_first_of(kWhitespaceWide);
  size_t chop_index = std::min(dot_index, ws_index);
  std::wstring min_string;
  if (chop_index == std::wstring::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = l10n_util::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  base::i18n::AdjustStringForLocaleDirection(min_string, &min_string);
  return min_string;
}
