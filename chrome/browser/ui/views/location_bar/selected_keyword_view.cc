// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar/selected_keyword_view.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
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

  bool is_extension_keyword;
  const std::wstring short_name = profile_->GetTemplateURLModel()->
      GetKeywordShortName(keyword, &is_extension_keyword);
  int message_id = is_extension_keyword ?
      IDS_OMNIBOX_EXTENSION_KEYWORD_TEXT : IDS_OMNIBOX_KEYWORD_TEXT;
  full_label_.SetText(l10n_util::GetStringF(message_id, short_name));
  const std::wstring min_string(
      location_bar_util::CalculateMinString(short_name));
  partial_label_.SetText(min_string.empty() ?
      full_label_.GetText() :
      l10n_util::GetStringF(message_id, min_string));
}
