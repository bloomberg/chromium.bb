// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/new_credit_card_bubble_views.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// The space between the bubble and edges of the web contents when showing
// without an anchor (e.g. when requestAutocomplete() is called from a popup).
const int kAnchorlessEndPadding = 20;
const int kAnchorlessTopPadding = 10;

// Get the view this bubble will be anchored to via |controller|.
views::View* GetAnchor(NewCreditCardBubbleController* controller) {
  Browser* browser = chrome::FindTabbedBrowser(controller->profile(), false);
  if (!browser)
    return NULL;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->GetToolbarView()->app_menu_button();
}

views::BubbleBorder::Arrow GetArrow(NewCreditCardBubbleController* controller) {
  views::View* anchor = GetAnchor(controller);
  return anchor ? views::BubbleBorder::TOP_RIGHT : views::BubbleBorder::NONE;
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

gfx::Size NewCreditCardBubbleViews::GetPreferredSize() const {
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

gfx::Rect NewCreditCardBubbleViews::GetBubbleBounds() {
  gfx::Rect bounds = views::BubbleDelegateView::GetBubbleBounds();
  if (GetAnchorView())
    return bounds;

  Browser* browser = chrome::FindBrowserWithProfile(controller_->profile());
  DCHECK(browser);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* contents_view = browser_view->GetContentsView();
  gfx::Rect web_contents_bounds = contents_view->GetBoundsInScreen();
  gfx::Insets border_insets(GetBubbleFrameView()->bubble_border()->GetInsets());

  int x;
  if (base::i18n::IsRTL()) {
    x = web_contents_bounds.x() - border_insets.left() + kAnchorlessEndPadding;
  } else {
    x = web_contents_bounds.right() + border_insets.right();
    x -= bounds.width() + kAnchorlessEndPadding;
  }
  int y = web_contents_bounds.y() - border_insets.top() + kAnchorlessTopPadding;

  int width = bounds.width() - border_insets.width();
  int height = bounds.height() - border_insets.height();

  return gfx::Rect(gfx::Point(x, y), gfx::Size(width, height));
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
    : BubbleDelegateView(GetAnchor(controller), GetArrow(controller)),
      controller_(controller),
      weak_ptr_factory_(this) {
}

}  // namespace autofill
