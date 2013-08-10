// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/new_credit_card_bubble_views.h"

#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// Get the view this bubble will be anchored to via |controller|.
views::View* GetAnchor(NewCreditCardBubbleController* controller) {
  Browser* browser = chrome::FindTabbedBrowser(controller->profile(), false,
                                               chrome::GetActiveDesktop());
  if (!browser)
    return NULL;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->GetToolbarView()->app_menu();
}

}  // namespace

NewCreditCardBubbleViews::~NewCreditCardBubbleViews() {
  controller_->OnBubbleDestroyed();
}

void NewCreditCardBubbleViews::Show() {
  // TODO(dbeam): investigate why this steals focus from the web contents.
  views::BubbleDelegateView::CreateBubble(this)->Show();

  // This bubble doesn't render correctly on Windows without calling
  // |SizeToContents()|. This must be called after showing the widget.
  SizeToContents();
}

void NewCreditCardBubbleViews::Hide() {
  GetWidget()->Close();
}

gfx::Size NewCreditCardBubbleViews::GetPreferredSize() {
  return gfx::Size(
      NewCreditCardBubbleView::kContentsWidth,
      GetHeightForWidth(NewCreditCardBubbleView::kContentsWidth));
}

void NewCreditCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        views::kRelatedControlVerticalSpacing));

  views::View* card_container = new views::View();
  card_container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 10));

  views::View* card_desc_view = new views::View();
  card_desc_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 10));

  views::ImageView* card_icon = new views::ImageView();
  const CreditCardDescription& card_desc = controller_->CardDescription();
  card_icon->SetImage(card_desc.icon.AsImageSkia());
  card_desc_view->AddChildView(card_icon);

  views::Label* card_name = new views::Label(card_desc.name);
  card_name->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  card_desc_view->AddChildView(card_name);
  card_container->AddChildView(card_desc_view);

  views::Label* desc = new views::Label(card_desc.description);
  desc->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  desc->SetMultiLine(true);
  card_container->AddChildView(desc);

  AddChildView(card_container);

  views::Link* link = new views::Link(controller_->LinkText());
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->set_listener(this);
  AddChildView(link);
}

base::string16 NewCreditCardBubbleViews::GetWindowTitle() const {
  return controller_->TitleText();
}

void NewCreditCardBubbleViews::LinkClicked(views::Link* source,
                                           int event_flags) {
  controller_->OnLinkClicked();
}

// static
base::WeakPtr<NewCreditCardBubbleView> NewCreditCardBubbleView::Create(
    NewCreditCardBubbleController* controller) {
  NewCreditCardBubbleViews* bubble = new NewCreditCardBubbleViews(controller);
  return bubble->weak_ptr_factory_.GetWeakPtr();
}

NewCreditCardBubbleViews::NewCreditCardBubbleViews(
    NewCreditCardBubbleController* controller)
    : BubbleDelegateView(GetAnchor(controller), views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      weak_ptr_factory_(this) {
  gfx::Insets insets = views::BubbleFrameView::GetTitleInsets();
  set_margins(gfx::Insets(0, insets.left(), insets.top(), insets.left()));
}

}  // namespace autofill
