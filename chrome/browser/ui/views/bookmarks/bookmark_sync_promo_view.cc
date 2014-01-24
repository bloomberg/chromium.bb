// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_sync_promo_view.h"

#include "base/strings/string16.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {
// Background color of the promo.
const SkColor kBackgroundColor = SkColorSetRGB(245, 245, 245);

// Color of the top border of the promo.
const SkColor kBorderColor = SkColorSetRGB(229, 229, 229);

// Width of the top border of the promo.
const int kBorderWidth = 1;

// Color of the text of the promo.
const SkColor kTextColor = SkColorSetRGB(102, 102, 102);

}  // namespace

BookmarkSyncPromoView::BookmarkSyncPromoView(BookmarkBubbleDelegate* delegate)
    : delegate_(delegate) {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  SetBorder(views::Border::CreateSolidSidedBorder(
      kBorderWidth, 0, 0, 0, kBorderColor));
  size_t offset;
  base::string16 link_text =
      l10n_util::GetStringUTF16(IDS_BOOKMARK_SYNC_PROMO_LINK);
  base::string16 promo_text = l10n_util::GetStringFUTF16(
      IDS_BOOKMARK_SYNC_PROMO_MESSAGE,
      link_text,
      &offset);

  views::StyledLabel* promo_label = new views::StyledLabel(promo_text, this);
  promo_label->SetDisplayedOnBackgroundColor(kBackgroundColor);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.font_style = gfx::Font::NORMAL;
  promo_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                             link_style);

  views::StyledLabel::RangeStyleInfo promo_style;
  promo_style.color = kTextColor;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    promo_label->AddStyleRange(before_link_range, promo_style);
  gfx::Range after_link_range(offset + link_text.length(), promo_text.length());
  if (!after_link_range.is_empty())
    promo_label->AddStyleRange(after_link_range, promo_style);

  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  views::kButtonHEdgeMarginNew,
                                                  views::kPanelVertMargin,
                                                  0);
  SetLayoutManager(layout);
  AddChildView(promo_label);
}

void BookmarkSyncPromoView::StyledLabelLinkClicked(const gfx::Range& range,
                                                   int event_flags) {
  delegate_->OnSignInLinkClicked();
}
