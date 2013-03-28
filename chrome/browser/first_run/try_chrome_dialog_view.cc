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
#include "chrome/installer/util/user_experiment.h"
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
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

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

const int kRadioGroupID = 1;

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
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::ImageView* icon = new views::ImageView();
  icon->SetImage(rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_32).ToImageSkia());
  gfx::Size icon_size = icon->GetPreferredSize();

  popup_ = new views::Widget;
  if (!popup_) {
    NOTREACHED();
    return DIALOG_ERROR;
  }
  // An approximate window size. After Layout() we'll get better bounds.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.can_activate = true;
  params.bounds = gfx::Rect(310, 200);
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

  // First row: [icon][pad][text][pad][button].
  columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::FIXED, icon_size.width(),
                     icon_size.height());
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
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

  // Optional fourth row: [divider]
  columns = layout->AddColumnSet(6);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Optional fifth row [checkbox][pad][button]
  columns = layout->AddColumnSet(7);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // First row.
  layout->StartRow(0, 0);
  layout->AddView(icon);

  // Find out what experiment we are conducting.
  installer::ExperimentDetails experiment;
  if (!BrowserDistribution::GetDistribution()->HasUserExperiments() ||
      !installer::CreateExperimentDetails(flavor_, &experiment) ||
      !experiment.heading) {
    NOTREACHED() << "Cannot determine which headline to show.";
    return DIALOG_ERROR;
  }
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(experiment.heading));
  label->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  label->SetMultiLine(true);
  label->SizeToFit(200);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(label);
  // The close button is custom.
  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::STATE_NORMAL,
                         rb.GetNativeImageNamed(IDR_CLOSE_2).ToImageSkia());
  close_button->SetImage(views::CustomButton::STATE_HOVERED,
                         rb.GetNativeImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  close_button->SetImage(views::CustomButton::STATE_PRESSED,
                         rb.GetNativeImageNamed(IDR_CLOSE_2_P).ToImageSkia());
  close_button->set_tag(BT_CLOSE_BUTTON);
  layout->AddView(close_button);

  // Second row.
  layout->StartRowWithPadding(0, 1, 0, 10);
  try_chrome_ = new views::RadioButton(
      l10n_util::GetStringUTF16(IDS_TRY_TOAST_TRY_OPT), kRadioGroupID);
  try_chrome_->SetChecked(true);
  try_chrome_->set_tag(BT_TRY_IT_RADIO);
  try_chrome_->set_listener(this);
  layout->AddView(try_chrome_);

  // Decide if the don't bug me is a button or a radio button.
  bool dont_bug_me_button =
      !!(experiment.flags & installer::kToastUiDontBugMeAsButton);

  // Optional third and fourth row.
  if (!dont_bug_me_button) {
    layout->StartRow(0, 1);
    dont_try_chrome_ = new views::RadioButton(
        l10n_util::GetStringUTF16(IDS_TRY_TOAST_CANCEL), kRadioGroupID);
    dont_try_chrome_->set_tag(BT_DONT_BUG_RADIO);
    dont_try_chrome_->set_listener(this);
    layout->AddView(dont_try_chrome_);
  }
  if (experiment.flags & installer::kToastUiUninstall) {
    layout->StartRow(0, 2);
    kill_chrome_ = new views::RadioButton(
        l10n_util::GetStringUTF16(IDS_UNINSTALL_CHROME), kRadioGroupID);
    layout->AddView(kill_chrome_);
  }

  views::Button* accept_button = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_OK));
  accept_button->set_tag(BT_OK_BUTTON);

  views::Separator* separator = NULL;
  if (experiment.flags & installer::kToastUiMakeDefault) {
    // In this flavor we have some veritical space, then a separator line
    // and the 'make default' checkbox and the OK button on the same row.
    layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
    layout->StartRow(0, 6);
    separator = new views::Separator;
    layout->AddView(separator);
    layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

    layout->StartRow(0, 7);
    make_default_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_TRY_TOAST_SET_DEFAULT));
    make_default_->SetChecked(true);
    layout->AddView(make_default_);
    layout->AddView(accept_button);
  } else {
    // On this other flavor there is no checkbox, the OK button and possibly
    // the cancel button are in the same row.
    layout->StartRowWithPadding(0, dont_bug_me_button ? 3 : 5, 0, 10);
    layout->AddView(accept_button);
    if (dont_bug_me_button) {
      // The dialog needs a "Don't bug me" as a button or as a radio button,
      // this the button case.
      views::Button* cancel_button = new views::NativeTextButton(
          this, l10n_util::GetStringUTF16(IDS_TRY_TOAST_CANCEL));
      cancel_button->set_tag(BT_CLOSE_BUTTON);
      layout->AddView(cancel_button);
    }
  }

  if (experiment.flags & installer::kToastUiWhyLink) {
    layout->StartRowWithPadding(0, 4, 0, 10);
    views::Link* link = new views::Link(
        l10n_util::GetStringUTF16(IDS_TRY_TOAST_WHY));
    link->set_listener(this);
    layout->AddView(link);
  }

  // We resize the window according to the layout manager. This takes into
  // account the differences between XP and Vista fonts and buttons.
  layout->Layout(root_view);
  gfx::Size preferred = layout->GetPreferredSize(root_view);
  if (separator) {
    int separator_height = separator->GetPreferredSize().height();
    separator->SetSize(gfx::Size(preferred.width(), separator_height));
  }

  gfx::Rect pos = ComputeWindowPosition(preferred.width(), preferred.height(),
                                        base::i18n::IsRTL());
  popup_->SetBounds(pos);

  // Carve the toast shape into the window.
  HWND toast_window;
#if defined(USE_AURA)
  toast_window =
      popup_->GetNativeView()->GetRootWindow()->GetAcceleratedWidget();
#else
  toast_window = popup_->GetNativeView();
#endif
  SetToastRegion(toast_window, preferred.width(), preferred.height());

  // Time to show the window in a modal loop. The ProcessSingleton should
  // already be locked and it will not process WM_COPYDATA requests. Change the
  // window to bring to foreground if a request arrives.
  CHECK(process_singleton->locked());
  process_singleton->SetActiveModalDialog(popup_->GetNativeView());
  popup_->Show();
  MessageLoop::current()->Run();
  process_singleton->SetActiveModalDialog(NULL);
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
      make_default_->SetVisible(false);
    }
    return;
  } else if (sender->tag() == BT_TRY_IT_RADIO) {
    if (make_default_) {
      make_default_->SetVisible(true);
      make_default_->SetChecked(true);
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

  if (make_default_) {
    if ((result_ == TRY_CHROME) && make_default_->checked())
        result_ = TRY_CHROME_AS_DEFAULT;
  }

  popup_->Close();
  MessageLoop::current()->Quit();
}

void TryChromeDialogView::LinkClicked(views::Link* source, int event_flags) {
  ::ShellExecuteW(NULL, L"open", kHelpCenterUrl, NULL, NULL, SW_SHOW);
}
