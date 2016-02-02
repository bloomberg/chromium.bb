// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"

#include <stddef.h>

#include "base/strings/string16.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Color of the text of the promo.
const SkColor kTextColor = SkColorSetRGB(102, 102, 102);

}  // namespace

BubbleSyncPromoView::BubbleSyncPromoView(BubbleSyncPromoDelegate* delegate,
                                         int link_text_resource_id,
                                         int message_text_resource_id)
    : delegate_(delegate) {
  size_t offset = 0;
  base::string16 link_text = l10n_util::GetStringUTF16(link_text_resource_id);
  base::string16 promo_text =
      l10n_util::GetStringFUTF16(message_text_resource_id, link_text, &offset);

  views::StyledLabel* promo_label = new views::StyledLabel(promo_text, this);

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

  SetLayoutManager(new views::FillLayout());
  AddChildView(promo_label);
}

BubbleSyncPromoView::~BubbleSyncPromoView() {}

void BubbleSyncPromoView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  delegate_->OnSignInLinkClicked();
}

const char* BubbleSyncPromoView::GetClassName() const {
  return "BubbleSyncPromoView";
}
