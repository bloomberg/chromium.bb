// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/proximity_auth/proximity_auth_error_bubble_view.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/proximity_auth/proximity_auth_error_bubble.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The bubble's content area width, in device-independent pixels.
const int kBubbleWidth = 296;

// The padding between columns in the bubble.
const int kBubbleIntraColumnPadding = 6;

}  // namespace

void ShowProximityAuthErrorBubble(const base::string16& message,
                                  const gfx::Range& link_range,
                                  const GURL& link_url,
                                  const gfx::Rect& anchor_rect,
                                  content::WebContents* web_contents) {
  // The created bubble is owned by the views hierarchy.
  new ProximityAuthErrorBubbleView(
      message, link_range, link_url, anchor_rect, web_contents);
}

ProximityAuthErrorBubbleView::ProximityAuthErrorBubbleView(
    const base::string16& message,
    const gfx::Range& link_range,
    const GURL& link_url,
    const gfx::Rect& anchor_rect,
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      link_url_(link_url) {
  SetAnchorRect(anchor_rect);
  set_arrow(views::BubbleBorder::LEFT_TOP);

  // Define this grid layout for the bubble:
  // ----------------------------
  // | icon | padding | message |
  // ----------------------------
  scoped_ptr<views::GridLayout> layout(new views::GridLayout(this));
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kBubbleIntraColumnPadding);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  // Construct the views.
  scoped_ptr<views::ImageView> warning_icon(new views::ImageView());
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  warning_icon->SetImage(*bundle.GetImageSkiaNamed(IDR_WARNING));

  scoped_ptr<views::StyledLabel> label(new views::StyledLabel(message, this));
  if (!link_range.is_empty()) {
    label->AddStyleRange(link_range,
                         views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  label->SizeToFit(
      kBubbleWidth - margins().width() - warning_icon->size().width() -
      kBubbleIntraColumnPadding);

  // Lay out the views.
  layout->StartRow(0, 0);
  layout->AddView(warning_icon.release());
  layout->AddView(label.release());
  SetLayoutManager(layout.release());

  views::Widget* widget = views::BubbleDelegateView::CreateBubble(this);
  widget->Show();
}

ProximityAuthErrorBubbleView::~ProximityAuthErrorBubbleView() {
}

void ProximityAuthErrorBubbleView::WebContentsDestroyed() {
  GetWidget()->Close();
}

void ProximityAuthErrorBubbleView::StyledLabelLinkClicked(
    const gfx::Range& range,
    int event_flags) {
  if (!web_contents())
    return;

  web_contents()->OpenURL(content::OpenURLParams(
      link_url_, content::Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}
