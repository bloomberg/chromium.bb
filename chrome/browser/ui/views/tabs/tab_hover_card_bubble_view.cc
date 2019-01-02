// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

TabHoverCardBubbleView::TabHoverCardBubbleView(views::View* anchor_view,
                                               TabRendererData data)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  set_can_activate(false);

  title_label_ = new views::Label();
  domain_label_ = new views::Label();
  UpdateCardContent(data);

  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(false);
  title_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::MEDIUM));
  title_label_->SetEnabledColor(gfx::kGoogleGrey800);
  domain_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_label_->SetMultiLine(false);
  domain_label_->SetEnabledColor(gfx::kGoogleGrey500);

  AddChildView(title_label_);
  AddChildView(domain_label_);
  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::Show() {
  widget_->Show();
}

void TabHoverCardBubbleView::Hide() {
  widget_->Hide();
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
}

void TabHoverCardBubbleView::UpdateCardAnchor(View* tab) {
  views::BubbleDialogDelegateView::SetAnchorView(tab);
}

int TabHoverCardBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  const gfx::Size size = BubbleDialogDelegateView::CalculatePreferredSize();
  return gfx::Size(240, size.height());
}
