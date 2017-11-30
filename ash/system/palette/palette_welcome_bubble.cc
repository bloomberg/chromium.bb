// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_welcome_bubble.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_tray.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Width of the bubble content label size.
constexpr int kBubbleContentLabelPreferredWidthDp = 380;

}  // namespace

// View which contains two text (one title) and a close button for closing the
// bubble. Controlled by PaletteWelcomeBubble and anchored to a PaletteTray.
class PaletteWelcomeBubble::WelcomeBubbleView
    : public views::BubbleDialogDelegateView,
      public views::ButtonListener {
 public:
  WelcomeBubbleView(views::View* anchor, views::BubbleBorder::Arrow arrow)
      : views::BubbleDialogDelegateView(anchor, arrow) {
    set_close_on_deactivate(true);
    set_can_activate(false);
    set_accept_events(true);
    set_parent_window(
        anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
            kShellWindowId_SettingBubbleContainer));
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical));

    // Add the header which contains the title and close button.
    auto* header = new views::View();
    auto* box_layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    header->SetLayoutManager(box_layout);
    AddChildView(header);

    // Add the title, which is bolded.
    auto* title = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_TITLE));
    title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title->SetFontList(views::Label::GetDefaultFontList().Derive(
        0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD));
    header->AddChildView(title);
    box_layout->SetFlexForView(title, 1);

    // Add a button to close the bubble.
    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kWindowControlCloseIcon, SK_ColorBLACK));
    header->AddChildView(close_button_);

    auto* content = new views::Label(l10n_util::GetStringUTF16(
        IDS_ASH_STYLUS_WARM_WELCOME_BUBBLE_DESCRIPTION));
    content->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    content->SetMultiLine(true);
    content->SizeToFit(kBubbleContentLabelPreferredWidthDp);
    AddChildView(content);

    views::BubbleDialogDelegateView::CreateBubble(this);
  }

  ~WelcomeBubbleView() override = default;

  views::ImageButton* close_button() { return close_button_; }

  // ui::BubbleDialogDelegateView:
  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == close_button_)
      GetWidget()->Close();
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return BubbleDialogDelegateView::CalculatePreferredSize();
  }

 private:
  views::ImageButton* close_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WelcomeBubbleView);
};

PaletteWelcomeBubble::PaletteWelcomeBubble(PaletteTray* tray) : tray_(tray) {
  Shell::Get()->session_controller()->AddObserver(this);
}

PaletteWelcomeBubble::~PaletteWelcomeBubble() {
  if (bubble_view_) {
    bubble_view_->GetWidget()->RemoveObserver(this);
    ShellPort::Get()->RemovePointerWatcher(this);
  }
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void PaletteWelcomeBubble::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShownPaletteWelcomeBubble, false);
}

void PaletteWelcomeBubble::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;
  ShellPort::Get()->RemovePointerWatcher(this);
}

void PaletteWelcomeBubble::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
}

void PaletteWelcomeBubble::ShowIfNeeded(bool shown_by_stylus) {
  if (!active_user_pref_service_)
    return;

  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  base::Optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  if (user_type && *user_type == user_manager::USER_TYPE_GUEST)
    return;

  if (!active_user_pref_service_->GetBoolean(
          prefs::kShownPaletteWelcomeBubble) &&
      !bubble_shown()) {
    Show(shown_by_stylus);
  }
}

void PaletteWelcomeBubble::MarkAsShown() {
  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kShownPaletteWelcomeBubble,
                                        true);
}

views::ImageButton* PaletteWelcomeBubble::GetCloseButtonForTest() {
  if (bubble_view_)
    return bubble_view_->close_button();

  return nullptr;
}

base::Optional<gfx::Rect> PaletteWelcomeBubble::GetBubbleBoundsForTest() {
  if (bubble_view_)
    return base::make_optional(bubble_view_->GetBoundsInScreen());

  return base::nullopt;
}

void PaletteWelcomeBubble::Show(bool shown_by_stylus) {
  if (!bubble_view_) {
    DCHECK(tray_);
    bubble_view_ =
        new WelcomeBubbleView(tray_, views::BubbleBorder::BOTTOM_RIGHT);
  }
  active_user_pref_service_->SetBoolean(prefs::kShownPaletteWelcomeBubble,
                                        true);
  bubble_view_->GetWidget()->Show();
  bubble_view_->GetWidget()->AddObserver(this);
  ShellPort::Get()->AddPointerWatcher(this,
                                      views::PointerWatcherEventTypes::BASIC);

  // If the bubble is shown after the device first reads a stylus, ignore the
  // first up event so the event responsible for showing the bubble does not
  // also cause the bubble to close.
  if (shown_by_stylus)
    ignore_stylus_event_ = true;
}

void PaletteWelcomeBubble::Hide() {
  if (bubble_view_)
    bubble_view_->GetWidget()->Close();
}

void PaletteWelcomeBubble::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    gfx::NativeView target) {
  if (!bubble_view_)
    return;

  // For devices with external stylus, the bubble is activated after it receives
  // a pen pointer event. However, on attaching |this| as a pointer watcher, it
  // receives the same set of events which activated the bubble in the first
  // place (ET_POINTER_DOWN, ET_POINTER_UP, ET_POINTER_CAPTURE_CHANGED). By only
  // looking at ET_POINTER_UP events and skipping the first up event if the
  // bubble is shown because of a stylus event, we ensure the same event that
  // opened the bubble does not close it as well.
  if (event.type() != ui::ET_POINTER_UP)
    return;

  if (ignore_stylus_event_) {
    ignore_stylus_event_ = false;
    return;
  }

  if (!bubble_view_->GetBoundsInScreen().Contains(location_in_screen))
    bubble_view_->GetWidget()->Close();
}

}  // namespace ash
