// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kMinimumTriggerDelay =
    base::TimeDelta::FromMilliseconds(50);
constexpr base::TimeDelta kMaximumTriggerDelay =
    base::TimeDelta::FromMilliseconds(1000);

// Hover card and preview image dimensions.
int GetPreferredTabHoverCardWidth() {
  return TabStyle::GetStandardWidth();
}

gfx::Size GetTabHoverCardPreviewImageSize() {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetPreferredTabHoverCardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

bool AreHoverCardImagesEnabled() {
  return base::FeatureList::IsEnabled(features::kTabHoverCardImages);
}

}  // namespace

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab, views::BubbleBorder::TOP_LEFT) {
  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);

  title_label_ =
      new views::Label(base::string16(), CONTEXT_TAB_HOVER_CARD_TITLE,
                       views::style::STYLE_PRIMARY);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(false);
  AddChildView(title_label_);

  domain_label_ = new views::Label(base::string16(), CONTEXT_BODY_TEXT_LARGE,
                                   ChromeTextStyle::STYLE_SECONDARY);
  domain_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_label_->SetMultiLine(false);
  AddChildView(domain_label_);

  if (AreHoverCardImagesEnabled()) {
    preview_image_ = new views::ImageView();
    preview_image_->SetVisible(AreHoverCardImagesEnabled());
    preview_image_->SetHorizontalAlignment(views::ImageViewBase::LEADING);
    AddChildView(preview_image_);
  }

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  constexpr int kOuterMargin = 12;
  constexpr int kLineSpacing = 8;
  title_label_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(kOuterMargin, kOuterMargin, kLineSpacing, kOuterMargin));
  domain_label_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(kLineSpacing, kOuterMargin, kOuterMargin, kOuterMargin));

  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateAndShow(Tab* tab) {
  UpdateCardContent(tab->data());

  views::BubbleDialogDelegateView::SetAnchorView(tab);

  // Start trigger timer if necessary.
  if (!widget_->IsVisible()) {
    // Note that this will restart the timer if it is already running. If the
    // hover cards are not yet visible, moving the cursor within the tabstrip
    // will not trigger the hover cards.
    delayed_show_timer_.Start(FROM_HERE, GetDelay(tab->width()), this,
                              &TabHoverCardBubbleView::ShowImmediately);
  }
}

void TabHoverCardBubbleView::Hide() {
  delayed_show_timer_.Stop();
  widget_->Hide();
}

int TabHoverCardBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

base::TimeDelta TabHoverCardBubbleView::GetDelay(int tab_width) const {
  // Delay is calculated as a logarithmic scale and bounded by a minimum width
  // based on the width of a pinned tab and a maximum of the standard width.
  //
  //  delay (ms)
  //           |
  // max delay-|                                    *
  //           |                          *
  //           |                    *
  //           |                *
  //           |            *
  //           |         *
  //           |       *
  //           |     *
  //           |    *
  // min delay-|****
  //           |___________________________________________ tab width
  //               |                                |
  //       pinned tab width               standard tab width
  if (tab_width < TabStyle::GetPinnedWidth())
    return kMinimumTriggerDelay;
  double logarithmic_fraction =
      std::log(tab_width - TabStyle::GetPinnedWidth() + 1) /
      std::log(TabStyle::GetStandardWidth() - TabStyle::GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor = kMaximumTriggerDelay - kMinimumTriggerDelay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + kMinimumTriggerDelay;
  return delay;
}

void TabHoverCardBubbleView::ShowImmediately() {
  widget_->Show();
}

void TabHoverCardBubbleView::UpdateCardContent(TabRendererData data) {
  title_label_->SetText(data.title);

  base::string16 domain = url_formatter::FormatUrl(
      data.url,
      url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitHTTP |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
  domain_label_->SetText(domain);

  if (preview_image_) {
    // Get the largest version of the favicon available.
    gfx::ImageSkia max_favicon = gfx::ImageSkia::CreateFrom1xBitmap(
        data.favicon.GetRepresentation(data.favicon.GetMaxSupportedScale())
            .GetBitmap());
    preview_image_->SetImage(max_favicon);
    const gfx::Size favicon_size = max_favicon.size();

    const gfx::Size preferred_size = GetTabHoverCardPreviewImageSize();

    // Scale the favicon to an appropriate size for the tab hover card.
    //
    // This is reasonably aesthetic for favicons, though it does not necessarily
    // fill up the entire width of the hover card. When we move to using
    // og:images or screenshots, we'll have to do something more sophisticated.
    if (!favicon_size.IsEmpty()) {
      float scale =
          float{preferred_size.height()} / float{favicon_size.height()};
      preview_image_->SetImageSize(gfx::Size(
          std::roundf(scale * favicon_size.width()), preferred_size.height()));
    }
  }
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(GetPreferredTabHoverCardWidth());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}
