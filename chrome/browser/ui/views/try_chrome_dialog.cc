// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog.h"

#include <shellapi.h>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/installer/util/experiment.h"
#include "chrome/installer/util/experiment_metrics.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr unsigned int kToastWidth = 360;
constexpr int kHoverAboveTaskbarHeight = 24;

const SkColor kBackgroundColor = SkColorSetRGB(0x1F, 0x1F, 0x1F);
const SkColor kHeaderColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);
const SkColor kBodyColor = SkColorSetARGB(0xAD, 0xFF, 0xFF, 0xFF);
const SkColor kBorderColor = SkColorSetARGB(0x80, 0x80, 0x80, 0x80);
const SkColor kButtonTextColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);
const SkColor kButtonAcceptColor = SkColorSetRGB(0x00, 0x78, 0xDA);
const SkColor kButtonNoThanksColor = SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF);

enum class ButtonTag { CLOSE_BUTTON, OK_BUTTON, NO_THANKS_BUTTON };

// Experiment specification information needed for layout.
// TODO(skare): Suppress x-to-close in relevant variations.
// TODO(skare): Implement hover behavior for x-to-close.
struct ExperimentVariations {
  // Resource ID for header message string.
  int heading_id;
  // Resource ID for body message string, or 0 for no body text.
  int body_id;
  // Whether the dialog has a 'no thanks' button. Dialog will always have a
  // close 'x'.
  bool has_no_thanks_button;
  // Which action to take on acceptance of the dialog.
  TryChromeDialog::Result result;
};

constexpr ExperimentVariations kExperiments[] = {
    {IDS_WIN10_TOAST_RECOMMENDATION, 0, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0, true,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0, false,
     TryChromeDialog::OPEN_CHROME_WELCOME_WIN10},
    {IDS_WIN10_TOAST_RECOMMENDATION, 0, false,
     TryChromeDialog::OPEN_CHROME_WELCOME},
    {IDS_WIN10_TOAST_RECOMMENDATION, IDS_WIN10_TOAST_SWITCH_FAST, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, IDS_WIN10_TOAST_SWITCH_SECURE, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_RECOMMENDATION, IDS_WIN10_TOAST_SWITCH_SMART, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_FAST, IDS_WIN10_TOAST_RECOMMENDATION, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SECURE, IDS_WIN10_TOAST_RECOMMENDATION, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SMART, IDS_WIN10_TOAST_RECOMMENDATION, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_BROWSE_FAST, 0, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_BROWSE_SAFELY, 0, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_BROWSE_SMART, 0, false,
     TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SMART_AND_SECURE, IDS_WIN10_TOAST_RECOMMENDATION,
     true, TryChromeDialog::OPEN_CHROME_DEFAULT},
    {IDS_WIN10_TOAST_SWITCH_SMART_AND_SECURE, IDS_WIN10_TOAST_RECOMMENDATION,
     true, TryChromeDialog::OPEN_CHROME_DEFAULT}};

// Whether a button is an accept or cancel-style button.
enum class TryChromeButtonType { OPEN_CHROME, NO_THANKS };

// Builds a Win10-styled rectangular button, for this toast displayed outside of
// the browser.
std::unique_ptr<views::LabelButton> CreateWin10StyleButton(
    views::ButtonListener* listener,
    const base::string16& text,
    TryChromeButtonType button_type) {
  auto button = base::MakeUnique<views::LabelButton>(listener, text,
                                                     CONTEXT_WINDOWS10_NATIVE);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  button->SetBackground(views::CreateSolidBackground(
      button_type == TryChromeButtonType::OPEN_CHROME ? kButtonAcceptColor
                                                      : kButtonNoThanksColor));
  button->SetEnabledTextColors(kButtonTextColor);
  // Request specific 32pt height, 160+pt width.
  button->SetMinSize(gfx::Size(160, 32));
  button->SetMaxSize(gfx::Size(0, 32));
  return button;
}

}  // namespace

// static
TryChromeDialog::Result TryChromeDialog::Show(
    size_t group,
    const ActiveModalDialogListener& listener) {
  if (group >= arraysize(kExperiments)) {
    // This is a test value. We want to make sure we exercise
    // returning this early. See TryChromeDialogBrowserTest test.
    return NOT_NOW;
  }

  TryChromeDialog dialog(group);
  return dialog.ShowDialog(listener, DialogType::MODAL, UsageType::FOR_CHROME);
}

TryChromeDialog::TryChromeDialog(size_t group)
    : usage_type_(UsageType::FOR_CHROME),
      group_(group),
      popup_(nullptr),
      result_(NOT_NOW) {}

TryChromeDialog::~TryChromeDialog() {}

TryChromeDialog::Result TryChromeDialog::ShowDialog(
    const ActiveModalDialogListener& listener,
    DialogType dialog_type,
    UsageType usage_type) {
  usage_type_ = usage_type;

  auto icon = base::MakeUnique<views::ImageView>();
  icon->SetImage(gfx::CreateVectorIcon(kInactiveToastLogoIcon, kHeaderColor));
  gfx::Size icon_size = icon->GetPreferredSize();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  // An approximate window size. Layout() can adjust.
  params.bounds = gfx::Rect(kToastWidth, 120);
  popup_ = new views::Widget;
  popup_->Init(params);

  views::View* root_view = popup_->GetRootView();
  root_view->SetBackground(views::CreateSolidBackground(kBackgroundColor));
  views::GridLayout* layout = views::GridLayout::CreatePanel(root_view);
  layout->set_minimum_size(gfx::Size(kToastWidth, 0));
  views::ColumnSet* columns;

  // Note the right padding is smaller than other dimensions,
  // to acommodate the close 'x' button.
  static constexpr gfx::Insets kInsets(10, 10, 12, 3);
  root_view->SetBorder(views::CreatePaddedBorder(
      views::CreateSolidBorder(1, kBorderColor), kInsets));

  static constexpr int kLabelSpacing = 10;
  static constexpr int kSpaceBetweenButtons = 4;
  static constexpr int kSpacingAfterHeadingHorizontal = 9;

  // First row: [icon][pad][text][pad][close button].
  // Left padding is accomplished via border.
  columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::FIXED, icon_size.width(),
                     icon_size.height());
  columns->AddPaddingColumn(0, kLabelSpacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kSpacingAfterHeadingHorizontal);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::TRAILING,
                     0, views::GridLayout::USE_PREF, 0, 0);
  int icon_padding = icon_size.width() + kLabelSpacing;

  // Optional second row: [pad][text].
  columns = layout->AddColumnSet(1);
  columns->AddPaddingColumn(0, icon_padding);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // Third row: [pad][optional button][pad][button].
  columns = layout->AddColumnSet(2);
  columns->AddPaddingColumn(0, 12 - kInsets.left());
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kSpaceBetweenButtons);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 12 - kInsets.right());

  // Padding between the top of the toast and first row is handled via border.
  // First row.
  layout->StartRow(0, 0);
  layout->AddView(icon.release());
  // All variants have a main header.
  auto header = base::MakeUnique<views::Label>(
      l10n_util::GetStringUTF16(kExperiments[group_].heading_id),
      CONTEXT_WINDOWS10_NATIVE);
  header->SetBackgroundColor(kBackgroundColor);
  header->SetEnabledColor(kHeaderColor);
  header->SetMultiLine(true);
  header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(header.release());

  // The close button is custom.
  auto close_button = base::MakeUnique<views::ImageButton>(this);
  close_button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kInactiveToastCloseIcon, kBodyColor));
  close_button->set_tag(static_cast<int>(ButtonTag::CLOSE_BUTTON));
  layout->AddView(close_button.release());

  // Second row: May have text or may be blank.
  layout->StartRow(0, 1);
  const int body_string_id = kExperiments[group_].body_id;
  if (body_string_id) {
    auto body_text = base::MakeUnique<views::Label>(
        l10n_util::GetStringUTF16(body_string_id), CONTEXT_WINDOWS10_NATIVE);
    body_text->SetBackgroundColor(kBackgroundColor);
    body_text->SetEnabledColor(kBodyColor);
    layout->AddView(body_text.release());
  }

  // Third row: one or two buttons depending on group.
  layout->StartRowWithPadding(0, 2, 0, 12);
  if (!kExperiments[group_].has_no_thanks_button)
    layout->SkipColumns(1);
  auto accept_button = CreateWin10StyleButton(
      this, l10n_util::GetStringUTF16(IDS_WIN10_TOAST_OPEN_CHROME),
      TryChromeButtonType::OPEN_CHROME);
  accept_button->set_tag(static_cast<int>(ButtonTag::OK_BUTTON));
  layout->AddView(accept_button.release());

  if (kExperiments[group_].has_no_thanks_button) {
    auto no_thanks_button = CreateWin10StyleButton(
        this, l10n_util::GetStringUTF16(IDS_WIN10_TOAST_NO_THANKS),
        TryChromeButtonType::NO_THANKS);
    no_thanks_button->set_tag(static_cast<int>(ButtonTag::NO_THANKS_BUTTON));
    layout->AddView(no_thanks_button.release());
  }

  // Padding between buttons and the edge of the view is via the border.
  gfx::Size preferred = layout->GetPreferredSize(root_view);
  gfx::Rect pos = ComputePopupBounds(preferred, base::i18n::IsRTL());
  popup_->SetBounds(pos);
  layout->Layout(root_view);

  // Update pre-show stats.
  time_shown_ = base::TimeTicks::Now();

  if (usage_type_ == UsageType::FOR_CHROME) {
    auto lock = storage_.AcquireLock();
    installer::Experiment experiment;
    if (lock->LoadExperiment(&experiment)) {
      experiment.SetDisplayTime(base::Time::Now());
      experiment.SetToastCount(experiment.toast_count() + 1);
      // TODO(skare): SetToastLocation via checking pinned state.
      // TODO(skare): SetUserSessionUptime
      lock->StoreExperiment(experiment);
    }
  }

  popup_->Show();
  if (!listener.is_null())
    listener.Run(popup_->GetNativeView());

  if (dialog_type == DialogType::MODAL) {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    if (!listener.is_null())
      listener.Run(nullptr);
  }
  return result_;
}

gfx::Rect TryChromeDialog::ComputePopupBounds(const gfx::Size& size,
                                              bool is_RTL) {
  gfx::Point origin;

  gfx::Rect work_area = popup_->GetWorkAreaBoundsInScreen();
  origin.set_x(is_RTL ? work_area.x() : work_area.right() - size.width());
  origin.set_y(work_area.bottom() - size.height() - kHoverAboveTaskbarHeight);

  return gfx::Rect(origin, size);
}

void TryChromeDialog::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender->tag() == static_cast<int>(ButtonTag::CLOSE_BUTTON) ||
      sender->tag() == static_cast<int>(ButtonTag::NO_THANKS_BUTTON)) {
    result_ = NOT_NOW;
  } else if (sender->tag() == static_cast<int>(ButtonTag::OK_BUTTON)) {
    result_ = kExperiments[group_].result;
  } else {
    NOTREACHED() << "Unknown button selected.";
  }

  // Update post-action stats.
  if (usage_type_ == UsageType::FOR_CHROME) {
    auto lock = storage_.AcquireLock();
    installer::Experiment experiment;
    if (lock->LoadExperiment(&experiment)) {
      base::TimeDelta action_delay = (base::TimeTicks::Now() - time_shown_);
      experiment.SetActionDelay(action_delay);
      if (sender->tag() == static_cast<int>(ButtonTag::CLOSE_BUTTON)) {
        experiment.SetState(installer::ExperimentMetrics::kSelectedClose);
      } else if (sender->tag() ==
                 static_cast<int>(ButtonTag::NO_THANKS_BUTTON)) {
        experiment.SetState(installer::ExperimentMetrics::kSelectedNoThanks);
      } else {
        // TODO(skare): Differentiate crash/no-crash/logoff cases.
        experiment.SetState(
            installer::ExperimentMetrics::kSelectedOpenChromeAndNoCrash);
      }
      lock->StoreExperiment(experiment);
    }
  }

  popup_->Close();
  popup_ = nullptr;
  run_loop_->QuitWhenIdle();
}
