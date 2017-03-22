// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_tray.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Padding for tray icon (dp; the button that shows the palette menu).
constexpr int kTrayIconMainAxisInset = 8;
constexpr int kTrayIconCrossAxisInset = 0;

// Width of the palette itself (dp).
constexpr int kPaletteWidth = 332;

// Padding at the top/bottom of the palette (dp).
constexpr int kPalettePaddingOnTop = 4;
constexpr int kPalettePaddingOnBottom = 2;

// Margins between the title view and the edges around it (dp).
constexpr int kPaddingBetweenTitleAndLeftEdge = 12;
constexpr int kPaddingBetweenTitleAndSeparator = 3;

// Color of the separator.
const SkColor kPaletteSeparatorColor = SkColorSetARGB(0x1E, 0x00, 0x00, 0x00);

// Returns true if we are in a user session that can show the stylus tools.
bool IsInUserSession() {
  SessionController* session_controller = Shell::Get()->session_controller();
  return !session_controller->IsUserSessionBlocked() &&
         session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         Shell::Get()->system_tray_delegate()->GetUserLoginStatus() !=
             LoginStatus::KIOSK_APP;
}

class TitleView : public views::View, public views::ButtonListener {
 public:
  explicit TitleView(PaletteTray* palette_tray) : palette_tray_(palette_tray) {
    // TODO(tdanderson|jdufault): Use TriView to handle the layout of the title.
    // See crbug.com/614453.
    auto* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    SetLayoutManager(box_layout);

    auto* title_label =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(title_label);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::TITLE);
    style.SetupLabel(title_label);
    box_layout->SetFlexForView(title_label, 1);
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      help_button_ =
          new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                               kSystemMenuHelpIcon, IDS_ASH_STATUS_TRAY_HELP);
      settings_button_ = new SystemMenuButton(
          this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
          IDS_ASH_PALETTE_SETTINGS);
    } else {
      gfx::ImageSkia help_icon =
          gfx::CreateVectorIcon(kSystemMenuHelpIcon, kMenuIconColor);
      gfx::ImageSkia settings_icon =
          gfx::CreateVectorIcon(kSystemMenuSettingsIcon, kMenuIconColor);

      auto* help_button = new ash::TrayPopupHeaderButton(
          this, help_icon, IDS_ASH_STATUS_TRAY_HELP);
      help_button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_HELP));
      help_button_ = help_button;

      auto* settings_button = new ash::TrayPopupHeaderButton(
          this, settings_icon, IDS_ASH_STATUS_TRAY_SETTINGS);
      settings_button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SETTINGS));
      settings_button_ = settings_button;
    }

    AddChildView(help_button_);
    AddChildView(settings_button_);
  }

  ~TitleView() override {}

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == settings_button_) {
      palette_tray_->RecordPaletteOptionsUsage(
          PaletteTrayOptions::PALETTE_SETTINGS_BUTTON);
      Shell::Get()->system_tray_controller()->ShowPaletteSettings();
      palette_tray_->HidePalette();
    } else if (sender == help_button_) {
      palette_tray_->RecordPaletteOptionsUsage(
          PaletteTrayOptions::PALETTE_HELP_BUTTON);
      Shell::Get()->system_tray_controller()->ShowPaletteHelp();
      palette_tray_->HidePalette();
    } else {
      NOTREACHED();
    }
  }

  // Unowned pointers to button views so we can determine which button was
  // clicked.
  views::View* settings_button_;
  views::View* help_button_;
  PaletteTray* palette_tray_;

  DISALLOW_COPY_AND_ASSIGN(TitleView);
};

}  // namespace

PaletteTray::PaletteTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf, true),
      palette_tool_manager_(new PaletteToolManager(this)),
      weak_factory_(this) {
  PaletteTool::RegisterToolInstances(palette_tool_manager_.get());

  if (MaterialDesignController::IsShelfMaterial())
    SetInkDropMode(InkDropMode::ON);

  SetLayoutManager(new views::FillLayout());
  icon_ = new views::ImageView();
  UpdateTrayIcon();

  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  tray_container()->AddChildView(icon_);

  Shell::GetInstance()->AddShellObserver(this);
  Shell::Get()->session_controller()->AddSessionStateObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

PaletteTray::~PaletteTray() {
  if (bubble_)
    bubble_->bubble_view()->reset_delegate();

  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::Get()->session_controller()->RemoveSessionStateObserver(this);
}

bool PaletteTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    if (num_actions_in_bubble_ == 0)
      RecordPaletteOptionsUsage(PaletteTrayOptions::PALETTE_CLOSED_NO_ACTION);
    HidePalette();
    return true;
  }

  return ShowPalette();
}

bool PaletteTray::ShowPalette() {
  if (bubble_)
    return false;

  DCHECK(tray_container());

  views::TrayBubbleView::InitParams init_params(GetAnchorAlignment(),
                                                kPaletteWidth, kPaletteWidth);
  init_params.can_activate = true;
  init_params.close_on_deactivate = true;

  DCHECK(tray_container());

  // The views::TrayBubbleView ctor will cause a shelf auto hide update check.
  // Make sure to block auto hiding before that check happens.
  should_block_shelf_auto_hide_ = true;

  // TODO(tdanderson): Refactor into common row layout code.
  // TODO(tdanderson|jdufault): Add material design ripple effects to the menu
  // rows.

  // Create and customize bubble view.
  views::TrayBubbleView* bubble_view =
      views::TrayBubbleView::Create(GetBubbleAnchor(), this, &init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());
  bubble_view->set_margins(
      gfx::Insets(kPalettePaddingOnTop, 0, kPalettePaddingOnBottom, 0));

  // Add title.
  auto* title_view = new TitleView(this);
  title_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kPaddingBetweenTitleAndLeftEdge, 0, 0)));
  bubble_view->AddChildView(title_view);

  // Add horizontal separator.
  views::Separator* separator = new views::Separator();
  separator->SetColor(kPaletteSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      kPaddingBetweenTitleAndSeparator, 0, kMenuSeparatorVerticalPadding, 0)));
  bubble_view->AddChildView(separator);

  // Add palette tools.
  // TODO(tdanderson|jdufault): Use SystemMenuButton to get the material design
  // ripples.
  std::vector<PaletteToolView> views = palette_tool_manager_->CreateViews();
  for (const PaletteToolView& view : views)
    bubble_view->AddChildView(view.view);

  // Show the bubble.
  bubble_.reset(new ash::TrayBubbleWrapper(this, bubble_view));
  SetIsActive(true);
  return true;
}

bool PaletteTray::ContainsPointInScreen(const gfx::Point& point) {
  if (icon_ && icon_->GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

void PaletteTray::SessionStateChanged(session_manager::SessionState state) {
  UpdateIconVisibility();
}

void PaletteTray::OnLockStateChanged(bool locked) {
  UpdateIconVisibility();

  // The user can eject the stylus during the lock screen transition, which will
  // open the palette. Make sure to close it if that happens.
  if (locked)
    HidePalette();
}

void PaletteTray::ClickedOutsideBubble() {
  if (num_actions_in_bubble_ == 0)
    RecordPaletteOptionsUsage(PaletteTrayOptions::PALETTE_CLOSED_NO_ACTION);
  HidePalette();
}

base::string16 PaletteTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE);
}

void PaletteTray::HideBubbleWithView(const views::TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HidePalette();
}

void PaletteTray::OnTouchscreenDeviceConfigurationChanged() {
  UpdateIconVisibility();
}

void PaletteTray::OnStylusStateChanged(ui::StylusState stylus_state) {
  // Device may have a stylus but it has been forcibly disabled.
  if (!palette_utils::HasStylusInput())
    return;

  PaletteDelegate* palette_delegate = Shell::GetInstance()->palette_delegate();

  // Don't do anything if the palette should not be shown or if the user has
  // disabled it all-together.
  if (!IsInUserSession() || !palette_delegate->ShouldShowPalette())
    return;

  // Auto show/hide the palette if allowed by the user.
  if (palette_delegate->ShouldAutoOpenPalette()) {
    if (stylus_state == ui::StylusState::REMOVED && !bubble_) {
      is_bubble_auto_opened_ = true;
      ShowPalette();
    } else if (stylus_state == ui::StylusState::INSERTED && bubble_) {
      HidePalette();
    }
  }

  // Disable any active modes if the stylus has been inserted.
  if (stylus_state == ui::StylusState::INSERTED)
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
}

void PaletteTray::BubbleViewDestroyed() {
  palette_tool_manager_->NotifyViewsDestroyed();
  SetIsActive(false);
}

void PaletteTray::OnMouseEnteredView() {}

void PaletteTray::OnMouseExitedView() {}

base::string16 PaletteTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

void PaletteTray::OnBeforeBubbleWidgetInit(
    views::Widget* anchor_widget,
    views::Widget* bubble_widget,
    views::Widget::InitParams* params) const {
  // Place the bubble in the same root window as |anchor_widget|.
  WmWindow::Get(anchor_widget->GetNativeWindow())
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          bubble_widget, kShellWindowId_SettingBubbleContainer, params);
}

void PaletteTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PaletteTray::HidePalette() {
  should_block_shelf_auto_hide_ = false;
  is_bubble_auto_opened_ = false;
  num_actions_in_bubble_ = 0;
  bubble_.reset();

  shelf()->UpdateAutoHideState();
}

void PaletteTray::HidePaletteImmediately() {
  if (bubble_)
    bubble_->bubble_widget()->SetVisibilityChangedAnimationsEnabled(false);
  HidePalette();
}

void PaletteTray::RecordPaletteOptionsUsage(PaletteTrayOptions option) {
  DCHECK_NE(option, PaletteTrayOptions::PALETTE_OPTIONS_COUNT);

  if (is_bubble_auto_opened_) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Shelf.Palette.Usage.AutoOpened", option,
                              PaletteTrayOptions::PALETTE_OPTIONS_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Ash.Shelf.Palette.Usage", option,
                              PaletteTrayOptions::PALETTE_OPTIONS_COUNT);
  }
}

void PaletteTray::RecordPaletteModeCancellation(PaletteModeCancelType type) {
  if (type == PaletteModeCancelType::PALETTE_MODE_CANCEL_TYPE_COUNT)
    return;

  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Shelf.Palette.ModeCancellation", type,
      PaletteModeCancelType::PALETTE_MODE_CANCEL_TYPE_COUNT);
}

bool PaletteTray::ShouldBlockShelfAutoHide() const {
  return should_block_shelf_auto_hide_;
}

void PaletteTray::OnActiveToolChanged() {
  ++num_actions_in_bubble_;
  UpdateTrayIcon();
}

WmWindow* PaletteTray::GetWindow() {
  return shelf()->GetWindow();
}

void PaletteTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;

  TrayBackgroundView::SetShelfAlignment(alignment);
}

void PaletteTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PaletteTray::Initialize() {
  PaletteDelegate* delegate = Shell::GetInstance()->palette_delegate();
  // |delegate| can be null in tests.
  if (!delegate)
    return;

  // OnPaletteEnabledPrefChanged will get called with the initial pref value,
  // which will take care of showing the palette.
  palette_enabled_subscription_ = delegate->AddPaletteEnableListener(base::Bind(
      &PaletteTray::OnPaletteEnabledPrefChanged, weak_factory_.GetWeakPtr()));
}

void PaletteTray::UpdateTrayIcon() {
  icon_->SetImage(CreateVectorIcon(
      palette_tool_manager_->GetActiveTrayIcon(
          palette_tool_manager_->GetActiveTool(ash::PaletteGroup::MODE)),
      kTrayIconSize, kShelfIconColor));
}

void PaletteTray::OnPaletteEnabledPrefChanged(bool enabled) {
  is_palette_enabled_ = enabled;

  if (!enabled) {
    SetVisible(false);
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
  } else {
    UpdateIconVisibility();
  }
}

void PaletteTray::UpdateIconVisibility() {
  SetVisible(is_palette_enabled_ && palette_utils::HasStylusInput() &&
             IsInUserSession());
}

}  // namespace ash
