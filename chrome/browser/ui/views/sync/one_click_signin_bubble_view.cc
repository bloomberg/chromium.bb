// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// Minimum width for the mutli-line label.
const int kMinimumDialogLabelWidth = 400;
const int kMinimumLabelWidth = 240;
const int kDialogMargin = 16;

namespace {

// The column set constants that can be used in the InitContent() function
// to layout views.
enum OneClickSigninBubbleColumnTypes {
  COLUMN_SET_FILL_ALIGN,
  COLUMN_SET_CONTROLS,
  COLUMN_SET_TITLE_BAR
};

class OneClickSigninDialogView : public OneClickSigninBubbleView {
 public:
  OneClickSigninDialogView(
      content::WebContents* web_content,
      views::View* anchor_view,
      const BrowserWindow::StartSyncCallback& start_sync_callback);

 private:
  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // Overridden from OneClickSigninBubbleView:
  virtual void InitContent(views::GridLayout* layout) OVERRIDE;
  virtual void GetButtons(views::TextButton** ok_button,
                          views::TextButton** undo_button) OVERRIDE;
  virtual views::Link* GetAdvancedLink() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  content::WebContents* web_content_;
  views::Link* learn_more_link_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninDialogView);
};

OneClickSigninDialogView::OneClickSigninDialogView(
    content::WebContents* web_content,
    views::View* anchor_view,
    const BrowserWindow::StartSyncCallback& start_sync_callback)
    : OneClickSigninBubbleView(anchor_view, start_sync_callback),
      web_content_(web_content),
      learn_more_link_(NULL),
      close_button_(NULL) {
  set_arrow_location(views::BubbleBorder::NONE);
  set_anchor_insets(gfx::Insets(0, 0, anchor_view->height() / 2, 0));
  set_close_on_deactivate(false);
  set_margins(gfx::Insets(kDialogMargin, kDialogMargin, kDialogMargin,
                          kDialogMargin));
}

ui::ModalType OneClickSigninDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void OneClickSigninDialogView::InitContent(views::GridLayout* layout) {
  // Column set for title bar.
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_TITLE_BAR);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  {
    layout->StartRow(0, COLUMN_SET_TITLE_BAR);

    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetFont(label->font().DeriveFont(3, gfx::Font::BOLD));
    layout->AddView(label);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::ImageButton::STATE_NORMAL,
                            rb.GetImageNamed(IDR_CLOSE_BAR).ToImageSkia());
    close_button_->SetImage(views::ImageButton::STATE_HOVERED,
                            rb.GetImageNamed(IDR_CLOSE_BAR_H).ToImageSkia());
    close_button_->SetImage(views::ImageButton::STATE_PRESSED,
                            rb.GetImageNamed(IDR_CLOSE_BAR_P).ToImageSkia());
    layout->AddView(close_button_);
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  {
    layout->StartRow(0, COLUMN_SET_FILL_ALIGN);

    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SizeToFit(kMinimumDialogLabelWidth);
    layout->AddView(label);

    layout->StartRow(0, COLUMN_SET_FILL_ALIGN);

    learn_more_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    learn_more_link_->set_listener(this);
    learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(learn_more_link_, 1, 1, views::GridLayout::TRAILING,
                    views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
}

void OneClickSigninDialogView::GetButtons(views::TextButton** ok_button,
                                          views::TextButton** undo_button) {
  *ok_button = new views::NativeTextButton(this);
  *undo_button = new views::NativeTextButton(this);

  // The default size of the buttons is too large.  To allow them to be smaller
  // ignore the minimum default size.  Furthermore, to make sure they are the
  // same size, SetText() is called with both strings on both buttons.
  (*ok_button)->set_ignore_minimum_size(true);
  (*undo_button)->set_ignore_minimum_size(true);
  string16 ok_label =
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON);
  string16 undo_label =
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_UNDO_BUTTON);
  (*ok_button)->SetText(undo_label);
  (*ok_button)->SetText(ok_label);
  (*undo_button)->SetText(ok_label);
  (*undo_button)->SetText(undo_label);
}

views::Link* OneClickSigninDialogView::GetAdvancedLink() {
  views::Link* advanced_link= new views::Link(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED));
  advanced_link->set_listener(this);
  advanced_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return advanced_link;
}

void OneClickSigninDialogView::LinkClicked(views::Link* source,
                                           int event_flags) {
  if (source == learn_more_link_) {
    content::OpenURLParams params(
        GURL(chrome::kChromeSyncLearnMoreURL), content::Referrer(),
        NEW_WINDOW, content::PAGE_TRANSITION_LINK, false);
    web_content_->OpenURL(params);
    return;
  }

  OneClickSigninBubbleView::LinkClicked(source, event_flags);
}

}  // namespace

// OneClickSigninBubbleView ----------------------------------------------------

// static
OneClickSigninBubbleView* OneClickSigninBubbleView::bubble_view_ = NULL;

// static
void OneClickSigninBubbleView::ShowBubble(
    BrowserWindow::OneClickSigninBubbleType type,
    ToolbarView* toolbar_view,
    const BrowserWindow::StartSyncCallback& start_sync) {
  if (IsShowing())
    return;

  bubble_view_ = type == BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE ?
      new OneClickSigninBubbleView(toolbar_view->app_menu(), start_sync) :
      new OneClickSigninDialogView(toolbar_view->GetWebContents(),
                                   toolbar_view->location_bar(), start_sync);

  views::BubbleDelegateView::CreateBubble(bubble_view_)->Show();
}

// static
bool OneClickSigninBubbleView::IsShowing() {
  return bubble_view_ != NULL;
}

// static
void OneClickSigninBubbleView::Hide() {
  if (IsShowing())
    bubble_view_->GetWidget()->Close();
}

OneClickSigninBubbleView::OneClickSigninBubbleView(
    views::View* anchor_view,
    const BrowserWindow::StartSyncCallback& start_sync_callback)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      advanced_link_(NULL),
      ok_button_(NULL),
      undo_button_(NULL),
      start_sync_callback_(start_sync_callback),
      message_loop_for_testing_(NULL) {
  DCHECK(!start_sync_callback_.is_null());
}

OneClickSigninBubbleView::~OneClickSigninBubbleView() {
}

void OneClickSigninBubbleView::AnimationEnded(const ui::Animation* animation) {
  views::BubbleDelegateView::AnimationEnded(animation);
  if (message_loop_for_testing_)
    message_loop_for_testing_->Quit();
}

void OneClickSigninBubbleView::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  set_border(views::Border::CreateEmptyBorder(8, 8, 8, 8));

  // Column set for descriptive text and link.
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_FILL_ALIGN);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                views::GridLayout::USE_PREF, 0, 0);

  // Column set for buttons at bottom of bubble.
  cs = layout->AddColumnSet(COLUMN_SET_CONTROLS);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);

  InitContent(layout);

  // Add controls at the bottom.
  advanced_link_= GetAdvancedLink();
  GetButtons(&ok_button_, &undo_button_);
  ok_button_->SetIsDefault(true);

  layout->StartRow(0, COLUMN_SET_CONTROLS);
  layout->AddView(advanced_link_);
  layout->AddView(ok_button_);
  layout->AddView(undo_button_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, 0));
}

void OneClickSigninBubbleView::InitContent(views::GridLayout* layout) {
  // Add main text description.
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_BUBBLE_MESSAGE));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kMinimumLabelWidth);

  layout->StartRow(0, COLUMN_SET_FILL_ALIGN);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
}

void OneClickSigninBubbleView::GetButtons(views::TextButton** ok_button,
                                          views::TextButton** undo_button) {
  *ok_button = new views::NativeTextButton(this);
  *undo_button = new views::NativeTextButton(this);

  // The default size of the buttons is too large.  To allow them to be smaller
  // ignore the minimum default size.  Furthermore, to make sure they are the
  // same size, SetText() is called with both strings on both buttons.
  (*ok_button)->set_ignore_minimum_size(true);
  (*undo_button)->set_ignore_minimum_size(true);
  string16 ok_label = l10n_util::GetStringUTF16(IDS_OK);
  string16 undo_label = l10n_util::GetStringUTF16(IDS_ONE_CLICK_BUBBLE_UNDO);
  (*ok_button)->SetText(undo_label);
  (*ok_button)->SetText(ok_label);
  (*undo_button)->SetText(ok_label);
  (*undo_button)->SetText(undo_label);
}

views::Link* OneClickSigninBubbleView::GetAdvancedLink() {
  views::Link* advanced_link= new views::Link(
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  advanced_link->set_listener(this);
  advanced_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return advanced_link;
}

void OneClickSigninBubbleView::WindowClosing() {
  // We have to reset |bubble_view_| here, not in our destructor, because
  // we'll be destroyed asynchronously and the shown state will be checked
  // before then.
  DCHECK_EQ(bubble_view_, this);
  bubble_view_ = NULL;

  if (!start_sync_callback_.is_null()) {
    base::ResetAndReturn(&start_sync_callback_).Run(
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }
}

bool OneClickSigninBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN ||
      accelerator.key_code() == ui::VKEY_ESCAPE) {
    StartFade(false);
    if (accelerator.key_code() == ui::VKEY_RETURN) {
      base::ResetAndReturn(&start_sync_callback_).Run(
          OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
    } else {
      start_sync_callback_.Reset();
    }

    return true;
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void OneClickSigninBubbleView::LinkClicked(views::Link* source,
                                           int event_flags) {
  StartFade(false);
  base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
}

void OneClickSigninBubbleView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  StartFade(false);
  base::ResetAndReturn(&start_sync_callback_).Run((sender == ok_button_) ?
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS :
      OneClickSigninSyncStarter::UNDO_SYNC);
}
