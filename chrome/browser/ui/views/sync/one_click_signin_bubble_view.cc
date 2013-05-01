// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
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
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// Minimum width for the multi-line label.
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
}  // namespace

// static
OneClickSigninBubbleView* OneClickSigninBubbleView::bubble_view_ = NULL;

// static
void OneClickSigninBubbleView::ShowBubble(
    BrowserWindow::OneClickSigninBubbleType type,
    const string16& email,
    const string16& error_message,
    ToolbarView* toolbar_view,
    const BrowserWindow::StartSyncCallback& start_sync) {
  if (IsShowing())
    return;

  switch (type) {
    case BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE:
      bubble_view_ = new OneClickSigninBubbleView(
          toolbar_view->GetWebContents(), toolbar_view->app_menu(),
          error_message, string16(), start_sync, false);
      break;
    case BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG:
      bubble_view_ = new OneClickSigninBubbleView(
          toolbar_view->GetWebContents(), toolbar_view->location_bar(),
          string16(), string16(), start_sync, true);
      break;
    case BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_SAML_MODAL_DIALOG:
      bubble_view_ = new OneClickSigninBubbleView(
          toolbar_view->GetWebContents(), toolbar_view->location_bar(),
          string16(), email, start_sync, true);
      break;
  }

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
    content::WebContents* web_contents,
    views::View* anchor_view,
    const string16& error_message,
    const string16& email,
    const BrowserWindow::StartSyncCallback& start_sync_callback,
    bool is_sync_dialog)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_contents_(web_contents),
      error_message_(error_message),
      email_(email),
      start_sync_callback_(start_sync_callback),
      is_sync_dialog_(is_sync_dialog),
      advanced_link_(NULL),
      learn_more_link_(NULL),
      ok_button_(NULL),
      undo_button_(NULL),
      close_button_(NULL),
      clicked_learn_more_(false),
      message_loop_for_testing_(NULL) {
  if (is_sync_dialog_) {
    DCHECK(!start_sync_callback_.is_null());
    set_arrow(views::BubbleBorder::NONE);
    set_anchor_view_insets(gfx::Insets(0, 0, anchor_view->height() / 2, 0));
    set_close_on_deactivate(false);
    set_margins(gfx::Insets(kDialogMargin, kDialogMargin, kDialogMargin,
                          kDialogMargin));
  }
}

OneClickSigninBubbleView::~OneClickSigninBubbleView() {
}

ui::ModalType OneClickSigninBubbleView::GetModalType() const {
  return is_sync_dialog_? ui::MODAL_TYPE_CHILD : ui::MODAL_TYPE_NONE;
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

  is_sync_dialog_ ? InitDialogContent(layout) : InitBubbleContent(layout);

  // Add controls at the bottom.
  InitAdvancedLink();

  layout->StartRow(0, COLUMN_SET_CONTROLS);
  layout->AddView(advanced_link_);

  InitButtons(layout);
  ok_button_->SetIsDefault(true);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, 0));
}

void OneClickSigninBubbleView::InitBubbleContent(views::GridLayout* layout) {
  // Add main text description.
  layout->StartRow(0, COLUMN_SET_FILL_ALIGN);

  views::Label* label = !error_message_.empty() ?
      new views::Label(error_message_) :
      new views::Label(
          l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_BUBBLE_MESSAGE));

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kMinimumLabelWidth);
  layout->AddView(label);

  layout->StartRow(0, COLUMN_SET_CONTROLS);

  InitLearnMoreLink();
  layout->AddView(learn_more_link_);
}

void OneClickSigninBubbleView::InitDialogContent(views::GridLayout* layout) {
  OneClickSigninHelper::LogConfirmHistogramValue(
      one_click_signin::HISTOGRAM_CONFIRM_SHOWN);

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

    views::Label* label = new views::Label(email_.empty() ?
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE) :
        l10n_util::GetStringFUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE_NEW,
                                   email_));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetFont(label->font().DeriveFont(3, gfx::Font::BOLD));
    layout->AddView(label);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::ImageButton::STATE_NORMAL,
                            rb.GetImageNamed(IDR_CLOSE_2).ToImageSkia());
    close_button_->SetImage(views::ImageButton::STATE_HOVERED,
                            rb.GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
    close_button_->SetImage(views::ImageButton::STATE_PRESSED,
                            rb.GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());

    layout->AddView(close_button_);
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  {
    layout->StartRow(0, COLUMN_SET_FILL_ALIGN);

    views::Label* label = new views::Label(email_.empty() ?
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE) :
        l10n_util::GetStringFUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE_NEW,
                                   email_));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SizeToFit(kMinimumDialogLabelWidth);
    layout->AddView(label);

    layout->StartRow(0, COLUMN_SET_FILL_ALIGN);

    InitLearnMoreLink();
    layout->AddView(learn_more_link_, 1, 1, views::GridLayout::TRAILING,
                    views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
}

void OneClickSigninBubbleView::InitButtons(views::GridLayout* layout) {
  GetButtons(&ok_button_, &undo_button_);
  layout->AddView(ok_button_);

  if (is_sync_dialog_)
    layout->AddView(undo_button_);
}

void OneClickSigninBubbleView::GetButtons(views::LabelButton** ok_button,
                                          views::LabelButton** undo_button) {
  *ok_button = new views::LabelButton(this, string16());
  (*ok_button)->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);

  // The default size of the buttons is too large.  To allow them to be smaller
  // ignore the minimum default size.,
  (*ok_button)->set_min_size(gfx::Size());

  string16 ok_label;

  if (is_sync_dialog_) {
    *undo_button = new views::LabelButton(this, string16());
    (*undo_button)->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
    (*undo_button)->set_min_size(gfx::Size());

    ok_label = l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON);
    string16 undo_label =
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_UNDO_BUTTON);

    // To make sure they are the same size, SetText() is called
    // with both strings on both buttons.
    (*ok_button)->SetText(undo_label);
    (*ok_button)->SetText(ok_label);
    (*undo_button)->SetText(ok_label);
    (*undo_button)->SetText(undo_label);
  } else {
    ok_label = l10n_util::GetStringUTF16(IDS_OK);
    (*ok_button)->SetText(ok_label);
  }
}

void OneClickSigninBubbleView::InitAdvancedLink() {
  advanced_link_ = is_sync_dialog_ ?
      new views::Link(
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED)):
      new views::Link(
        l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED));

  advanced_link_->set_listener(this);
  advanced_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void OneClickSigninBubbleView::InitLearnMoreLink() {
  learn_more_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->set_listener(this);
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

bool OneClickSigninBubbleView::AcceleratorPressed(
  const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN ||
      accelerator.key_code() == ui::VKEY_ESCAPE) {
    OneClickSigninBubbleView::Hide();

    if (is_sync_dialog_) {
      if (accelerator.key_code() == ui::VKEY_RETURN) {
        OneClickSigninHelper::LogConfirmHistogramValue(
        clicked_learn_more_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_RETURN :
            one_click_signin::HISTOGRAM_CONFIRM_RETURN);

        base::ResetAndReturn(&start_sync_callback_).Run(
          OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
      } else if (accelerator.key_code() == ui::VKEY_ESCAPE) {
        OneClickSigninHelper::LogConfirmHistogramValue(
        clicked_learn_more_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_ESCAPE :
            one_click_signin::HISTOGRAM_CONFIRM_ESCAPE);

        start_sync_callback_.Reset();
      }
    }

    return true;
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void OneClickSigninBubbleView::LinkClicked(views::Link* source,
                                           int event_flags) {
  if (source == learn_more_link_) {
    if (is_sync_dialog_ && !clicked_learn_more_) {
      OneClickSigninHelper::LogConfirmHistogramValue(
          one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE);
      clicked_learn_more_ = true;
    }

    WindowOpenDisposition location =
      is_sync_dialog_ ? NEW_WINDOW : NEW_FOREGROUND_TAB;

    content::OpenURLParams params(
        GURL(chrome::kChromeSyncLearnMoreURL), content::Referrer(),
        location, content::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params);

    // don't hide the modal dialog, as this is an informational link
    if (is_sync_dialog_)
      return;
  } else if (source == advanced_link_) {
    if (is_sync_dialog_) {
      OneClickSigninHelper::LogConfirmHistogramValue(
        clicked_learn_more_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED :
            one_click_signin::HISTOGRAM_CONFIRM_ADVANCED);

      base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
    } else {
      content::OpenURLParams params(
          GURL(chrome::kChromeUISettingsURL), content::Referrer(),
          CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
      web_contents_->OpenURL(params);
    }
  }

  Hide();
}

void OneClickSigninBubbleView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  Hide();

  if (is_sync_dialog_) {
    if (sender == ok_button_)
      OneClickSigninHelper::LogConfirmHistogramValue(
          clicked_learn_more_ ?
              one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_OK :
              one_click_signin::HISTOGRAM_CONFIRM_OK);

    if (sender == undo_button_)
      OneClickSigninHelper::LogConfirmHistogramValue(
          clicked_learn_more_ ?
              one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_UNDO :
              one_click_signin::HISTOGRAM_CONFIRM_UNDO);

    if (sender == close_button_)
      OneClickSigninHelper::LogConfirmHistogramValue(
          clicked_learn_more_ ?
              one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE :
              one_click_signin::HISTOGRAM_CONFIRM_CLOSE);

    base::ResetAndReturn(&start_sync_callback_).Run((sender == ok_button_) ?
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS :
      OneClickSigninSyncStarter::UNDO_SYNC);
  }
}

void OneClickSigninBubbleView::WindowClosing() {
  // We have to reset |bubble_view_| here, not in our destructor, because
  // we'll be destroyed asynchronously and the shown state will be checked
  // before then.
  DCHECK_EQ(bubble_view_, this);
  bubble_view_ = NULL;

  if (is_sync_dialog_ && !start_sync_callback_.is_null()) {
    base::ResetAndReturn(&start_sync_callback_).Run(
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }
}
