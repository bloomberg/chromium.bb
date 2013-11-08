// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/manage_passwords_bubble_view.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/manage_passwords_icon_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(browser->fullscreen_controller());
  DCHECK(!IsShowing());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  views::View* anchor_view = is_fullscreen ?
      NULL : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ =
      new ManagePasswordsBubbleView(anchor_view, web_contents);

  if (is_fullscreen) {
    manage_passwords_bubble_->set_parent_window(
        web_contents->GetView()->GetTopLevelNativeWindow());
  }

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }

  manage_passwords_bubble_->GetWidget()->Show();
}

// static
void ManagePasswordsBubbleView::CloseBubble() {
  if (manage_passwords_bubble_)
   manage_passwords_bubble_->Close();
}

// static
bool ManagePasswordsBubbleView::IsShowing() {
  // The bubble may be in the process of closing.
  return (manage_passwords_bubble_ != NULL) &&
      manage_passwords_bubble_->GetWidget()->IsVisible();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    views::View* anchor_view, content::WebContents* web_contents)
    : BubbleDelegateView(anchor_view, anchor_view ?
          views::BubbleBorder::TOP_RIGHT : views::BubbleBorder::NONE),
      web_contents_(web_contents) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_notify_enter_exit_on_child(true);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {}

void ManagePasswordsBubbleView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  // The bubble's padding from the screen edge, used in fullscreen.
  const int kFullscreenPaddingEnd = 20;
  const size_t bubble_half_width = width() / 2;
  const int x_pos = base::i18n::IsRTL() ?
      screen_bounds.x() + bubble_half_width + kFullscreenPaddingEnd :
      screen_bounds.right() - bubble_half_width - kFullscreenPaddingEnd;
  set_anchor_rect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
  // TODO(npentrel) change set_anchor_rect to update its view on being called
  // instead of using SizeToContents().
  SizeToContents();
}

void ManagePasswordsBubbleView::Close() {
  GetWidget()->Close();
}

void ManagePasswordsBubbleView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  if (sender == save_button_) {
    content_settings->set_password_action(PasswordFormManager::SAVE);
  } else {
    DCHECK_EQ(cancel_button_, sender);
    content_settings->set_password_action(PasswordFormManager::DO_NOTHING);
  }
  Close();
}

void ManagePasswordsBubbleView::Init() {
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const int kSingleColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kSingleColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  views::Label* title_label =
      new views::Label(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD));
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(0, kSingleColumnSetId);
  layout->AddView(title_label);

  const int kDoubleColumnSetId = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(kDoubleColumnSetId);

  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 1,
                               GridLayout::USE_PREF, 0, 0);
  double_column_set->AddPaddingColumn(
      0, views::kRelatedControlSmallVerticalSpacing);
  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                               GridLayout::USE_PREF, 0, 0);

  cancel_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
  cancel_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  save_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  save_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, kDoubleColumnSetId);
  layout->AddView(save_button_);
  layout->AddView(cancel_button_);
}

void ManagePasswordsBubbleView::WindowClosing() {
  // Close() closes the window asynchronously, so by the time we reach here,
  // |manage_passwords_bubble_| may have already been reset.
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}
