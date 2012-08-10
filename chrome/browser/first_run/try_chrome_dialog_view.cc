// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/try_chrome_dialog_view.h"

#include <shellapi.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

const wchar_t kHelpCenterUrl[] =
    L"https://www.google.com/support/chrome/bin/answer.py?answer=150752";

enum ButtonTags {
  BT_NONE,
  BT_CLOSE_BUTTON,
  BT_OK_BUTTON,
  BT_TRY_IT_RADIO,
  BT_DONT_BUG_RADIO
};

}  // namespace

// static
TryChromeDialogView::Result TryChromeDialogView::Show(
    size_t flavor,
    ProcessSingleton* process_singleton) {
  if (flavor > 10000) {
    // This is a test value. We want to make sure we exercise
    // returning this early. See TryChromeDialogBrowserTest test.
    return NOT_NOW;
  }
  TryChromeDialogView dialog(flavor);
  return dialog.ShowModal(process_singleton);
}

TryChromeDialogView::TryChromeDialogView(size_t flavor)
    : flavor_(flavor),
      popup_(NULL),
      try_chrome_(NULL),
      kill_chrome_(NULL),
      dont_try_chrome_(NULL),
      make_default_(NULL),
      result_(COUNT)  {
}

TryChromeDialogView::~TryChromeDialogView() {
}

TryChromeDialogView::Result TryChromeDialogView::ShowModal(
    ProcessSingleton* process_singleton) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  views::ImageView* icon = new views::ImageView();
  icon->SetImage(rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_32).ToImageSkia());
  gfx::Size icon_size = icon->GetPreferredSize();

  // An approximate window size. After Layout() we'll get better bounds.
  popup_ = new views::Widget;
  if (!popup_) {
    NOTREACHED();
    return DIALOG_ERROR;
  }

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.can_activate = true;
  params.bounds = gfx::Rect(310, 160);
  popup_->Init(params);

  views::View* root_view = popup_->GetRootView();
  // The window color is a tiny bit off-white.
  root_view->set_background(
      views::Background::CreateSolidBackground(0xfc, 0xfc, 0xfc));

  views::GridLayout* layout = views::GridLayout::CreatePanel(root_view);
  if (!layout) {
    NOTREACHED();
    return DIALOG_ERROR;
  }
  root_view->SetLayoutManager(layout);
  views::ColumnSet* columns;

  // First row: [icon][pad][text][button].
  columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::FIXED, icon_size.width(),
                     icon_size.height());
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Optional second row: [pad][pad][radio 1].
  columns = layout->AddColumnSet(1);
  columns->AddPaddingColumn(0, icon_size.width());
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Third row: [pad][pad][radio 2].
  columns = layout->AddColumnSet(2);
  columns->AddPaddingColumn(0, icon_size.width());
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Fourth row: [pad][pad][button][pad][button].
  columns = layout->AddColumnSet(3);
  columns->AddPaddingColumn(0, icon_size.width());
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  // Fifth row: [pad][pad][link].
  columns = layout->AddColumnSet(4);
  columns->AddPaddingColumn(0, icon_size.width());
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Optional fourth row: [button].
  columns = layout->AddColumnSet(5);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Optional fourth row: [pad][pad][checkbox].
  columns = layout->AddColumnSet(6);
  columns->AddPaddingColumn(0, icon_size.width());
  columns->AddPaddingColumn(0,
      views::kRelatedControlHorizontalSpacing + views::kPanelHorizIndentation);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  // First row views.
  layout->StartRow(0, 0);
  layout->AddView(icon);

  // Find out what experiment we are conducting.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!dist) {
    NOTREACHED() << "Cannot determine browser distribution";
    return DIALOG_ERROR;
  }
  BrowserDistribution::UserExperiment experiment;
  if (!dist->GetExperimentDetails(&experiment, flavor_) ||
      !experiment.heading) {
    NOTREACHED() << "Cannot determine which headline to show.";
    return DIALOG_ERROR;
  }
  string16 heading = l10n_util::GetStringUTF16(experiment.heading);
  views::Label* label = new views::Label(heading);
  label->SetFont(rb.GetFont(ResourceBundle::MediumBoldFont));
  label->SetMultiLine(true);
  label->SizeToFit(200);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  // The close button is custom.
  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                         rb.GetNativeImageNamed(IDR_CLOSE_BAR).ToImageSkia());
  close_button->SetImage(views::CustomButton::BS_HOT,
                         rb.GetNativeImageNamed(IDR_CLOSE_BAR_H).ToImageSkia());
  close_button->SetImage(views::CustomButton::BS_PUSHED,
                         rb.GetNativeImageNamed(IDR_CLOSE_BAR_P).ToImageSkia());
  close_button->set_tag(BT_CLOSE_BUTTON);
  layout->AddView(close_button);

  // Second row views.
  const string16 try_it(l10n_util::GetStringUTF16(IDS_TRY_TOAST_TRY_OPT));
  layout->StartRowWithPadding(0, 1, 0, 10);
  try_chrome_ = new views::RadioButton(try_it, 1);
  try_chrome_->SetChecked(true);
  try_chrome_->set_tag(BT_TRY_IT_RADIO);
  try_chrome_->set_listener(this);
  layout->AddView(try_chrome_);

  // Decide if the don't bug me is a button or a radio button.
  bool dont_bug_me_button =
      experiment.flags & BrowserDistribution::kDontBugMeAsButton ? true : false;

  // Optional third and fourth row views.
  if (!dont_bug_me_button) {
    layout->StartRow(0, 1);
    const string16 decline(l10n_util::GetStringUTF16(IDS_TRY_TOAST_CANCEL));
    dont_try_chrome_ = new views::RadioButton(decline, 1);
    dont_try_chrome_->set_tag(BT_DONT_BUG_RADIO);
    dont_try_chrome_->set_listener(this);
    layout->AddView(dont_try_chrome_);
  }
  if (experiment.flags & BrowserDistribution::kUninstall) {
    layout->StartRow(0, 2);
    const string16 kill_it(l10n_util::GetStringUTF16(IDS_UNINSTALL_CHROME));
    kill_chrome_ = new views::RadioButton(kill_it, 1);
    layout->AddView(kill_chrome_);
  }
  if (experiment.flags & BrowserDistribution::kMakeDefault) {
    layout->StartRow(0, 6);
    const string16 default_text(
        l10n_util::GetStringUTF16(IDS_TRY_TOAST_SET_DEFAULT));
    make_default_ = new views::Checkbox(default_text);
    gfx::Font font = make_default_->font().DeriveFont(0, gfx::Font::ITALIC);
    make_default_->SetFont(font);
    make_default_->SetChecked(true);
    layout->AddView(make_default_);
  }

  // Button row, the last or next to last depending on the 'why?' link.
  const string16 ok_it(l10n_util::GetStringUTF16(IDS_OK));
  views::Button* accept_button = new views::NativeTextButton(this, ok_it);
  accept_button->set_tag(BT_OK_BUTTON);

  layout->StartRowWithPadding(0, dont_bug_me_button ? 3 : 5, 0, 10);
  layout->AddView(accept_button);
  if (dont_bug_me_button) {
    // The bubble needs a "Don't bug me" as a button or as a radio button, this
    // this the button case.
    const string16 cancel_it(l10n_util::GetStringUTF16(IDS_TRY_TOAST_CANCEL));
    views::Button* cancel_button = new views::NativeTextButton(this, cancel_it);
    cancel_button->set_tag(BT_CLOSE_BUTTON);
    layout->AddView(cancel_button);
  }
  if (experiment.flags & BrowserDistribution::kWhyLink) {
    layout->StartRowWithPadding(0, 4, 0, 10);
    const string16 why_this(l10n_util::GetStringUTF16(IDS_TRY_TOAST_WHY));
    views::Link* link = new views::Link(why_this);
    link->set_listener(this);
    layout->AddView(link);
  }

  // We resize the window according to the layout manager. This takes into
  // account the differences between XP and Vista fonts and buttons.
  layout->Layout(root_view);
  gfx::Size preferred = layout->GetPreferredSize(root_view);
  gfx::Rect pos = ComputeWindowPosition(preferred.width(), preferred.height(),
                                        base::i18n::IsRTL());
  popup_->SetBounds(pos);

  // Carve the toast shape into the window.
  SetToastRegion(popup_->GetNativeView(),
                 preferred.width(), preferred.height());

  // Time to show the window in a modal loop. The ProcessSingleton should
  // already be locked and it will not process WM_COPYDATA requests. Change the
  // window to bring to foreground if a request arrives.
  CHECK(process_singleton->locked());
  process_singleton->SetForegroundWindow(popup_->GetNativeView());
  popup_->Show();
  MessageLoop::current()->Run();
  process_singleton->SetForegroundWindow(NULL);
  return result_;
}

gfx::Rect TryChromeDialogView::ComputeWindowPosition(int width,
                                                     int height,
                                                     bool is_RTL) {
  // The 'Shell_TrayWnd' is the taskbar. We like to show our window in that
  // monitor if we can. This code works even if such window is not found.
  HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", NULL);
  HMONITOR monitor = ::MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info = {sizeof(info)};
  if (!GetMonitorInfoW(monitor, &info)) {
    // Quite unexpected. Do a best guess at a visible rectangle.
    return gfx::Rect(20, 20, width + 20, height + 20);
  }
  // The |rcWork| is the work area. It should account for the taskbars that
  // are in the screen when we called the function.
  int left = is_RTL ? info.rcWork.left : info.rcWork.right - width;
  int top = info.rcWork.bottom - height;
  return gfx::Rect(left, top, width, height);
}

void TryChromeDialogView::SetToastRegion(HWND window, int w, int h) {
  static const POINT polygon[] = {
    {0,   4}, {1,   2}, {2,   1}, {4, 0},   // Left side.
    {w-4, 0}, {w-2, 1}, {w-1, 2}, {w, 4},   // Right side.
    {w, h}, {0, h}
  };
  HRGN region = ::CreatePolygonRgn(polygon, arraysize(polygon), WINDING);
  ::SetWindowRgn(window, region, FALSE);
}

void TryChromeDialogView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender->tag() == BT_DONT_BUG_RADIO) {
    if (make_default_) {
      make_default_->SetChecked(false);
      make_default_->SetState(views::CustomButton::BS_DISABLED);
    }
    return;
  } else if (sender->tag() == BT_TRY_IT_RADIO) {
    if (make_default_) {
      make_default_->SetChecked(true);
      make_default_->SetState(views::CustomButton::BS_NORMAL);
    }
    return;
  } else if (sender->tag() == BT_CLOSE_BUTTON) {
    // The user pressed cancel or the [x] button.
    result_ = NOT_NOW;
  } else if (!try_chrome_) {
    // We don't have radio buttons, the user pressed ok.
    result_ = TRY_CHROME;
  } else {
    // The outcome is according to the selected radio button.
    if (try_chrome_->checked())
      result_ = TRY_CHROME;
    else if (dont_try_chrome_ && dont_try_chrome_->checked())
      result_ = NOT_NOW;
    else if (kill_chrome_ && kill_chrome_->checked())
      result_ = UNINSTALL_CHROME;
    else
      NOTREACHED() << "Unknown radio button selected";
  }

  if ((result_ == TRY_CHROME) && make_default_->checked())
      result_ = TRY_CHROME_AS_DEFAULT;

  popup_->Close();
  MessageLoop::current()->Quit();
}

void TryChromeDialogView::LinkClicked(views::Link* source, int event_flags) {
  ::ShellExecuteW(NULL, L"open", kHelpCenterUrl, NULL, NULL, SW_SHOW);
}
