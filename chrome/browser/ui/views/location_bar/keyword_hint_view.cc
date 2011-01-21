// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/label.h"

// Amount of space to offset the tab image from the top of the view by.
static const int kTabImageYOffset = 4;

// The tab key image.
static const SkBitmap* kTabButtonBitmap = NULL;

KeywordHintView::KeywordHintView(Profile* profile) : profile_(profile) {
  leading_label_ = new views::Label();
  trailing_label_ = new views::Label();
  AddChildView(leading_label_);
  AddChildView(trailing_label_);

  if (!kTabButtonBitmap) {
    kTabButtonBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
  }
}

KeywordHintView::~KeywordHintView() {
}

void KeywordHintView::SetFont(const gfx::Font& font) {
  leading_label_->SetFont(font);
  trailing_label_->SetFont(font);
}

void KeywordHintView::SetColor(const SkColor& color) {
  leading_label_->SetColor(color);
  trailing_label_->SetColor(color);
}

void KeywordHintView::SetKeyword(const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  std::vector<size_t> content_param_offsets;
  bool is_extension_keyword;
  string16 short_name = profile_->GetTemplateURLModel()->
      GetKeywordShortName(WideToUTF16Hack(keyword), &is_extension_keyword);
  int message_id = is_extension_keyword ?
      IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
  const std::wstring keyword_hint =
      UTF16ToWide(l10n_util::GetStringFUTF16(
          message_id,
          string16(),
          short_name,
          &content_param_offsets));
  if (content_param_offsets.size() == 2) {
    leading_label_->SetText(
        keyword_hint.substr(0, content_param_offsets.front()));
    trailing_label_->SetText(
        keyword_hint.substr(content_param_offsets.front()));
  } else {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
  }
}

void KeywordHintView::Paint(gfx::Canvas* canvas) {
  int image_x = leading_label_->IsVisible() ? leading_label_->width() : 0;

  // Since we paint the button image directly on the canvas (instead of using a
  // child view), we must mirror the button's position manually if the locale
  // is right-to-left.
  gfx::Rect tab_button_bounds(image_x,
                              kTabImageYOffset,
                              kTabButtonBitmap->width(),
                              kTabButtonBitmap->height());
  tab_button_bounds.set_x(MirroredLeftPointForRect(tab_button_bounds));
  canvas->DrawBitmapInt(*kTabButtonBitmap,
                        tab_button_bounds.x(),
                        tab_button_bounds.y());
}

gfx::Size KeywordHintView::GetPreferredSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  gfx::Size prefsize = leading_label_->GetPreferredSize();
  int width = prefsize.width();
  width += kTabButtonBitmap->width();
  prefsize = trailing_label_->GetPreferredSize();
  width += prefsize.width();
  return gfx::Size(width, prefsize.height());
}

gfx::Size KeywordHintView::GetMinimumSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  return gfx::Size(kTabButtonBitmap->width(), 0);
}

void KeywordHintView::Layout() {
  // TODO(sky): baseline layout.
  bool show_labels = (width() != kTabButtonBitmap->width());

  leading_label_->SetVisible(show_labels);
  trailing_label_->SetVisible(show_labels);
  int x = 0;
  gfx::Size pref;

  if (show_labels) {
    pref = leading_label_->GetPreferredSize();
    leading_label_->SetBounds(x, 0, pref.width(), height());

    x += pref.width() + kTabButtonBitmap->width();
    pref = trailing_label_->GetPreferredSize();
    trailing_label_->SetBounds(x, 0, pref.width(), height());
  }
}
