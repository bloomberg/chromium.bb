// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog.h"

#include <shellapi.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/win/taskbar_icon_finder.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/installer/util/experiment.h"
#include "chrome/installer/util/experiment_storage.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
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

// TryChromeDialog::ModalShowDelegate ------------------------------------------

// A delegate for use by the modal Show() function to update the experiment
// state in the Windows registry and break out of the modal run loop upon
// completion.
class TryChromeDialog::ModalShowDelegate : public TryChromeDialog::Delegate {
 public:
  // Constructs the updater with a closure to be run after the dialog is closed
  // to break out of the modal run loop.
  explicit ModalShowDelegate(base::Closure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  ~ModalShowDelegate() override = default;

 protected:
  // TryChromeDialog::Delegate:
  void SetToastLocation(
      installer::ExperimentMetrics::ToastLocation toast_location) override;
  void SetExperimentState(installer::ExperimentMetrics::State state) override;
  void InteractionComplete() override;

 private:
  base::Closure quit_closure_;
  installer::ExperimentStorage storage_;

  // The time at which the toast was shown; used for computing the action delay.
  base::TimeTicks time_shown_;

  DISALLOW_COPY_AND_ASSIGN(ModalShowDelegate);
};

void TryChromeDialog::ModalShowDelegate::SetToastLocation(
    installer::ExperimentMetrics::ToastLocation toast_location) {
  time_shown_ = base::TimeTicks::Now();

  auto lock = storage_.AcquireLock();
  installer::Experiment experiment;
  if (lock->LoadExperiment(&experiment)) {
    experiment.SetDisplayTime(base::Time::Now());
    experiment.SetToastCount(experiment.toast_count() + 1);
    experiment.SetToastLocation(toast_location);
    // TODO(skare): SetUserSessionUptime
    lock->StoreExperiment(experiment);
  }
}

void TryChromeDialog::ModalShowDelegate::SetExperimentState(
    installer::ExperimentMetrics::State state) {
  auto lock = storage_.AcquireLock();
  installer::Experiment experiment;
  if (lock->LoadExperiment(&experiment)) {
    experiment.SetActionDelay(base::TimeTicks::Now() - time_shown_);
    experiment.SetState(state);
    lock->StoreExperiment(experiment);
  }
}

void TryChromeDialog::ModalShowDelegate::InteractionComplete() {
  quit_closure_.Run();
}

// TryChromeDialog -------------------------------------------------------------

// static
TryChromeDialog::Result TryChromeDialog::Show(
    size_t group,
    ActiveModalDialogListener listener) {
  if (group >= arraysize(kExperiments)) {
    // Exit immediately given bogus values; see TryChromeDialogBrowserTest test.
    return NOT_NOW;
  }

  base::RunLoop run_loop;
  ModalShowDelegate delegate(run_loop.QuitWhenIdleClosure());
  TryChromeDialog dialog(group, &delegate);

  dialog.ShowDialogAsync();

  if (listener) {
    listener.Run(base::Bind(&TryChromeDialog::OnProcessNotification,
                            base::Unretained(&dialog)));
  }
  run_loop.Run();
  if (listener)
    listener.Run(base::Closure());

  return dialog.result();
}

TryChromeDialog::TryChromeDialog(size_t group, Delegate* delegate)
    : group_(group), delegate_(delegate) {
  DCHECK_LT(group, arraysize(kExperiments));
  DCHECK(delegate);
}

TryChromeDialog::~TryChromeDialog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
}

void TryChromeDialog::ShowDialogAsync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // Get the bounding rectangle of Chrome's taskbar icon on the primary monitor.
  FindTaskbarIcon(base::BindOnce(&TryChromeDialog::OnTaskbarIconRect,
                                 base::Unretained(this)));
}

void TryChromeDialog::OnTaskbarIconRect(const gfx::Rect& icon_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // It's possible that a rendezvous from another browser process arrived while
  // searching for the taskbar icon (see OnProcessNotification). In this case,
  // report the DEFER result and bail out immediately.
  if (result_ == OPEN_CHROME_DEFER) {
    DCHECK_EQ(state_, installer::ExperimentMetrics::kOtherLaunch);
    CompleteInteraction();
    return;
  }

  // Create the popup.
  auto icon = base::MakeUnique<views::ImageView>();
  icon->SetImage(gfx::CreateVectorIcon(kInactiveToastLogoIcon, kHeaderColor));
  gfx::Size icon_size = icon->GetPreferredSize();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  // An approximate window size. Layout() can adjust.
  params.bounds = gfx::Rect(kToastWidth, 120);
  popup_ = new views::Widget;
  popup_->Init(params);
  popup_->AddObserver(this);

  views::View* root_view = popup_->GetRootView();
  root_view->SetBackground(views::CreateSolidBackground(kBackgroundColor));
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(root_view);
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

  installer::ExperimentMetrics::ToastLocation location =
      installer::ExperimentMetrics::kOverTaskbarPin;
  gfx::Rect bounds = ComputePopupBoundsOverTaskbarIcon(preferred, icon_rect);
  if (bounds.IsEmpty()) {
    location = installer::ExperimentMetrics::kOverNotificationArea;
    bounds = ComputePopupBoundsOverNoficationArea(preferred);
  }

  popup_->SetBounds(bounds);
  layout->Layout(root_view);

  popup_->Show();
  delegate_->SetToastLocation(location);
}

gfx::Rect TryChromeDialog::ComputePopupBoundsOverTaskbarIcon(
    const gfx::Size& size,
    const gfx::Rect& icon_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // Was no taskbar icon found?
  if (icon_rect.IsEmpty())
    return icon_rect;

  // Get the taskbar and its bounding rectangle (in DIP).
  RECT temp_rect = {};
  HWND taskbar = ::FindWindow(L"Shell_TrayWnd", nullptr);
  if (!taskbar || !::GetWindowRect(taskbar, &temp_rect))
    return gfx::Rect();
  gfx::Rect taskbar_rect =
      display::win::ScreenWin::ScreenToDIPRect(taskbar, gfx::Rect(temp_rect));

  // The taskbar is always on the primary display.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const gfx::Rect& monitor_rect = display.bounds();

  // Is the taskbar not on the primary display (e.g., is it hidden)?
  if (!monitor_rect.Contains(taskbar_rect))
    return gfx::Rect();

  // Center the window over/under/next to the taskbar icon, offset by a bit.
  static constexpr int kOffset = 11;
  gfx::Rect result(size);

  // Where is the taskbar? Assume that it's "wider" than it is "tall".
  if (taskbar_rect.width() > taskbar_rect.height()) {
    // Horizonal.
    result.set_x(icon_rect.x() + icon_rect.width() / 2 - size.width() / 2);
    if (taskbar_rect.y() < monitor_rect.y() + monitor_rect.height() / 2) {
      // Top.
      result.set_y(icon_rect.y() + icon_rect.height() + kOffset);
    } else {
      // Bottom.
      result.set_y(icon_rect.y() - size.height() - kOffset);
    }
  } else {
    // Vertical.
    result.set_y(icon_rect.y() + icon_rect.height() / 2 - size.height() / 2);
    if (taskbar_rect.x() < monitor_rect.x() + monitor_rect.width() / 2) {
      // Left.
      result.set_x(icon_rect.x() + icon_rect.width() + kOffset);
    } else {
      // Right.
      result.set_x(icon_rect.x() - size.width() - kOffset);
    }
  }

  // Make sure it doesn't spill out of the display's work area.
  // TODO: If Chrome's icon is near the edge of the display, this could move
  // the dialog off center. In this case, the arrow should stick to |icon_rect|
  // rather than stay centered in |result|.
  result.AdjustToFit(display.work_area());
  return result;
}

gfx::Rect TryChromeDialog::ComputePopupBoundsOverNoficationArea(
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  const bool is_RTL = base::i18n::IsRTL();
  const gfx::Rect work_area = popup_->GetWorkAreaBoundsInScreen();
  return gfx::Rect(
      is_RTL ? work_area.x() : work_area.right() - size.width(),
      work_area.bottom() - size.height() - kHoverAboveTaskbarHeight,
      size.width(), size.height());
}

void TryChromeDialog::CompleteInteraction() {
  delegate_->SetExperimentState(state_);
  delegate_->InteractionComplete();
}

void TryChromeDialog::OnProcessNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  // Another browser process is trying to rendezvous with this one, which is
  // either waiting on FindTaskbarIcon to complete or waiting on the user to
  // interact with the dialog. In the former case, no attempt is made to stop
  // the search, as it is expected to complete "quickly". When it does complete
  // (in OnTaskbarIconRect), processing will complete tout de suite. In the
  // latter case, the dialog is closed so that processing will continue in
  // OnWidgetDestroyed. OPEN_CHROME_DEFER conveys to this browser process that
  // it should ignore its own command line and instead handle that provided by
  // the other browser process.
  result_ = OPEN_CHROME_DEFER;
  state_ = installer::ExperimentMetrics::kOtherLaunch;
  if (popup_)
    popup_->Close();
}

void TryChromeDialog::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(result_, NOT_NOW);

  // Ignore this press if another press or a rendezvous has already been
  // registered.
  if (state_ != installer::ExperimentMetrics::kOtherClose)
    return;

  // Figure out what the subsequent action and experiment state should be based
  // on which button was pressed.
  switch (sender->tag()) {
    case static_cast<int>(ButtonTag::CLOSE_BUTTON):
      state_ = installer::ExperimentMetrics::kSelectedClose;
      break;
    case static_cast<int>(ButtonTag::OK_BUTTON):
      result_ = kExperiments[group_].result;
      state_ = installer::ExperimentMetrics::kSelectedOpenChromeAndNoCrash;
      break;
    case static_cast<int>(ButtonTag::NO_THANKS_BUTTON):
      state_ = installer::ExperimentMetrics::kSelectedNoThanks;
      break;
    default:
      NOTREACHED();
      break;
  }

  popup_->Close();
}

void TryChromeDialog::OnWidgetDestroyed(views::Widget* widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(widget, popup_);

  popup_->RemoveObserver(this);
  popup_ = nullptr;

  CompleteInteraction();
}
