// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog_view.h"

#include <shellapi.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/user_experiment.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace {

const wchar_t kHelpCenterUrl[] =
    L"https://support.google.com/chrome/answer/150752";

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
    const ActiveModalDialogListener& listener) {
  if (flavor > 10000) {
    // This is a test value. We want to make sure we exercise
    // returning this early. See TryChromeDialogBrowserTest test.
    return NOT_NOW;
  }
  TryChromeDialogView dialog(flavor);
  TryChromeDialogView::Result result = dialog.ShowDialog(
      listener, kDialogType::MODAL, kUsageType::FOR_CHROME);
  return result;
}

TryChromeDialogView::TryChromeDialogView(size_t flavor)
    : flavor_(flavor),
      popup_(NULL),
      try_chrome_(NULL),
      kill_chrome_(NULL),
      dont_try_chrome_(NULL),
      make_default_(NULL),
      result_(COUNT) {}

TryChromeDialogView::~TryChromeDialogView() {}

TryChromeDialogView::Result TryChromeDialogView::ShowDialog(
    const ActiveModalDialogListener& listener,
    kDialogType dialog_type,
    kUsageType usage_type) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::ImageView* icon = new views::ImageView();
  icon->SetImage(rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_32).ToImageSkia());
  gfx::Size icon_size = icon->GetPreferredSize();

  // An approximate window size. After Layout() we'll get better bounds.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.bounds = gfx::Rect(310, 200);
  popup_ = new views::Widget;
  popup_->Init(params);

  views::View* root_view = popup_->GetRootView();
  // The window color is a tiny bit off-white.
  root_view->SetBackground(
      views::CreateSolidBackground(SkColorSetRGB(0xfc, 0xfc, 0xfc)));

  views::GridLayout* layout = views::GridLayout::CreatePanel(root_view);
  views::ColumnSet* columns;

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int label_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int unrelated_space_horiz =
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  const int unrelated_space_vert =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  const int button_spacing_horiz =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // First row: [icon][pad][text][pad][button].
  columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::FIXED, icon_size.width(),
                     icon_size.height());
  columns->AddPaddingColumn(0, label_spacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, unrelated_space_horiz);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  int icon_padding = icon_size.width() + label_spacing;
  // Optional second row: [pad][radio 1].
  columns = layout->AddColumnSet(1);
  columns->AddPaddingColumn(0, icon_padding);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Third row: [pad][radio 2].
  columns = layout->AddColumnSet(2);
  columns->AddPaddingColumn(0, icon_padding);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Fourth row: [pad][button][pad][button].
  columns = layout->AddColumnSet(3);
  columns->AddPaddingColumn(0, icon_padding);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, button_spacing_horiz);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  // Fifth row: [pad][link].
  columns = layout->AddColumnSet(4);
  columns->AddPaddingColumn(0, icon_padding);
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
  columns->AddPaddingColumn(0, unrelated_space_horiz);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // First row.
  layout->StartRow(0, 0);
  layout->AddView(icon);

  // Find out what experiment we are conducting.
  installer::ExperimentDetails experiment;
  if ((usage_type != kUsageType::FOR_TESTING &&
          !install_static::SupportsRetentionExperiments()) ||
      !installer::CreateExperimentDetails(flavor_, &experiment) ||
      !experiment.heading) {
    NOTREACHED() << "Cannot determine which headline to show.";
    return DIALOG_ERROR;
  }
  views::Label* label =
      new views::Label(l10n_util::GetStringUTF16(experiment.heading),
                       views::style::CONTEXT_DIALOG_TITLE);
  label->SetMultiLine(true);
  label->SizeToFit(200);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(label);
  // The close button is custom.
  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::Button::STATE_NORMAL,
                         rb.GetNativeImageNamed(IDR_CLOSE_2).ToImageSkia());
  close_button->SetImage(views::Button::STATE_HOVERED,
                         rb.GetNativeImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  close_button->SetImage(views::Button::STATE_PRESSED,
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

  views::LabelButton* accept_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_OK));
  accept_button->set_tag(BT_OK_BUTTON);

  views::Separator* separator = NULL;
  if (experiment.flags & installer::kToastUiMakeDefault) {
    // In this flavor we have some vertical space, then a separator line
    // and the 'make default' checkbox and the OK button on the same row.
    layout->AddPaddingRow(0, unrelated_space_vert);
    layout->StartRow(0, 6);
    separator = new views::Separator();
    layout->AddView(separator);
    layout->AddPaddingRow(0, unrelated_space_vert);

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
      views::LabelButton* cancel_button =
          views::MdTextButton::CreateSecondaryUiButton(
              this, l10n_util::GetStringUTF16(IDS_TRY_TOAST_CANCEL));
      cancel_button->set_tag(BT_CLOSE_BUTTON);
      layout->AddView(cancel_button);
    }
  }

  if (experiment.flags & installer::kToastUiWhyLink) {
    layout->StartRowWithPadding(0, 4, 0, 10);
    views::Link* link =
        new views::Link(l10n_util::GetStringUTF16(IDS_TRY_TOAST_WHY));
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

  gfx::Rect pos = ComputeWindowPosition(preferred, base::i18n::IsRTL());
  popup_->SetBounds(pos);

  // Carve the toast shape into the window.
  HWND toast_window;
  toast_window = popup_->GetNativeView()->GetHost()->GetAcceleratedWidget();

  gfx::Size size_in_pixels = display::win::ScreenWin::DIPToScreenSize(
      toast_window, preferred);
  SetToastRegion(toast_window, size_in_pixels.width(),
                 size_in_pixels.height());

  // Time to show the window in a modal loop.
  popup_->Show();

  if (!listener.is_null())
    listener.Run(popup_->GetNativeView());

  // If the dialog is not modal, we don't control when it is going to be
  // dismissed and hence we cannot inform the listener about the dialog going
  // away.
  if (dialog_type == kDialogType::MODAL) {
    base::RunLoop().Run();
    if (!listener.is_null())
      listener.Run(nullptr);
  }
  return result_;
}

gfx::Rect TryChromeDialogView::ComputeWindowPosition(const gfx::Size& size,
                                                     bool is_RTL) {
  gfx::Point origin;

  gfx::Rect work_area = popup_->GetWorkAreaBoundsInScreen();
  origin.set_x(is_RTL ? work_area.x() : work_area.right() - size.width());
  origin.set_y(work_area.bottom()- size.height());

  return gfx::Rect(origin, size);
}

void TryChromeDialogView::SetToastRegion(HWND window, int w, int h) {
  static const POINT polygon[] = {
      {0, 4},     {1, 2},     {2, 1},     {4, 0},  // Left side.
      {w - 4, 0}, {w - 2, 1}, {w - 1, 2}, {w, 4},  // Right side.
      {w, h},     {0, h}};
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
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void TryChromeDialogView::LinkClicked(views::Link* source, int event_flags) {
  ::ShellExecuteW(NULL, L"open", kHelpCenterUrl, NULL, NULL, SW_SHOW);
}
