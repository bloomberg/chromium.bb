// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SelectedKeywordView::SelectedKeywordView(const int background_images[],
                                         int contained_image,
                                         SkColor color,
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
  SetLabel(((width() == GetPreferredSize().width()) ?
      full_label_ : partial_label_).text());
  IconLabelBubbleView::Layout();
}

void SelectedKeywordView::SetKeyword(const string16& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!model)
    return;

  bool is_extension_keyword;
  const string16 short_name = model->GetKeywordShortName(keyword,
                                                         &is_extension_keyword);
  const string16 full_name = is_extension_keyword ?
      short_name :
      l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT, short_name);
  full_label_.SetText(full_name);

  const string16 min_string(location_bar_util::CalculateMinString(short_name));
  const string16 partial_name = is_extension_keyword ?
      min_string :
      l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT, min_string);
  partial_label_.SetText(min_string.empty() ?
      full_label_.text() : partial_name);
}
