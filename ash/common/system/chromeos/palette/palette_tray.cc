// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_tray.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/metrics/histogram_macros.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Predefined padding for the icon used in this tray. These are to be set to the
// border of the icon, depending on the current |shelf_alignment()|.
const int kHorizontalShelfHorizontalPadding = 8;
const int kHorizontalShelfVerticalPadding = 4;
const int kVerticalShelfHorizontalPadding = 2;
const int kVerticalShelfVerticalPadding = 5;

// Width of the palette itself (dp).
const int kPaletteWidth = 332;

// Size of icon in the shelf (dp).
const int kShelfIconSize = 18;

// Margins between the title view and the edges around it (dp).
const int kPaddingBetweenTitleAndTopEdge = 4;
const int kPaddingBetweenTitleAndLeftEdge = 12;
const int kPaddingBetweenTitleAndSeparator = 3;

// Margin between the separator beneath the title and the first action (dp).
const int kPaddingActionsAndTopSeparator = 4;

// Size of the header icons (dp).
const int kIconSize = 20;

// Creates a separator.
views::Separator* CreateSeparator(views::Separator::Orientation orientation) {
  views::Separator* separator =
      new views::Separator(views::Separator::HORIZONTAL);
  separator->SetColor(ash::kBorderDarkColor);
  return separator;
}

// Returns true if we are in a user session that can show the stylus tools.
bool IsInUserSession() {
  SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  return !session_state_delegate->IsUserSessionBlocked() &&
         session_state_delegate->GetSessionState() ==
             SessionStateDelegate::SESSION_STATE_ACTIVE &&
         WmShell::Get()->system_tray_delegate()->GetUserLoginStatus() !=
             LoginStatus::KIOSK_APP;
}

class TitleView : public views::View, public views::ButtonListener {
 public:
  explicit TitleView(PaletteTray* palette_tray) : palette_tray_(palette_tray) {
    auto& rb = ui::ResourceBundle::GetSharedInstance();

    auto* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    SetLayoutManager(box_layout);

    views::Label* text_label =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE));
    text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    text_label->SetFontList(rb.GetFontList(ui::ResourceBundle::BoldFont));
    AddChildView(text_label);
    box_layout->SetFlexForView(text_label, 1);

    gfx::ImageSkia settings_icon =
        gfx::CreateVectorIcon(kSystemMenuSettingsIcon, kMenuIconColor);
    gfx::ImageSkia help_icon =
        gfx::CreateVectorIcon(kSystemMenuHelpIcon, kMenuIconColor);
    DCHECK_EQ(kIconSize, help_icon.width());

    help_button_ = new ash::TrayPopupHeaderButton(this, help_icon,
                                                  IDS_ASH_STATUS_TRAY_HELP);
    help_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_HELP));
    AddChildView(help_button_);

    settings_button_ = new ash::TrayPopupHeaderButton(
        this, settings_icon, IDS_ASH_STATUS_TRAY_SETTINGS);
    settings_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SETTINGS));
    AddChildView(settings_button_);
  }

  ~TitleView() override {}

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == settings_button_) {
      palette_tray_->RecordPaletteOptionsUsage(
          PaletteTrayOptions::PALETTE_SETTINGS_BUTTON);
      WmShell::Get()->system_tray_controller()->ShowPaletteSettings();
      palette_tray_->HidePalette();
    } else if (sender == help_button_) {
      palette_tray_->RecordPaletteOptionsUsage(
          PaletteTrayOptions::PALETTE_HELP_BUTTON);
      WmShell::Get()->system_tray_controller()->ShowPaletteHelp();
      palette_tray_->HidePalette();
    } else {
      NOTREACHED();
    }
  }

  // Unowned pointers to button views so we can determine which button was
  // clicked.
  ash::TrayPopupHeaderButton* settings_button_;
  ash::TrayPopupHeaderButton* help_button_;
  PaletteTray* palette_tray_;

  DISALLOW_COPY_AND_ASSIGN(TitleView);
};

}  // namespace

PaletteTray::PaletteTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      palette_tool_manager_(new PaletteToolManager(this)),
      weak_factory_(this) {
  // PaletteTray should only be instantiated if the palette feature is enabled.
  DCHECK(IsPaletteFeatureEnabled());

  PaletteTool::RegisterToolInstances(palette_tool_manager_.get());

  SetContentsBackground();

  SetLayoutManager(new views::FillLayout());
  icon_ = new views::ImageView();
  UpdateTrayIcon();

  SetIconBorderForShelfAlignment();
  tray_container()->AddChildView(icon_);

  WmShell::Get()->AddShellObserver(this);
  WmShell::Get()->GetSessionStateDelegate()->AddSessionStateObserver(this);
  if (WmShell::Get()->palette_delegate()) {
    WmShell::Get()->palette_delegate()->SetStylusStateChangedCallback(
        base::Bind(&PaletteTray::OnStylusStateChanged,
                   weak_factory_.GetWeakPtr()));
  }

  // OnPaletteEnabledPrefChanged will get called with the initial pref value,
  // which will take care of showing the palette.
  palette_enabled_subscription_ =
      WmShell::Get()->palette_delegate()->AddPaletteEnableListener(
          base::Bind(&PaletteTray::OnPaletteEnabledPrefChanged,
                     weak_factory_.GetWeakPtr()));
}

PaletteTray::~PaletteTray() {
  if (bubble_)
    bubble_->bubble_view()->reset_delegate();

  WmShell::Get()->RemoveShellObserver(this);
  WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(this);
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

  views::TrayBubbleView::InitParams init_params(
      views::TrayBubbleView::ANCHOR_TYPE_TRAY, GetAnchorAlignment(),
      kPaletteWidth, kPaletteWidth);
  init_params.first_item_has_no_margin = true;
  init_params.can_activate = true;
  init_params.close_on_deactivate = true;

  DCHECK(tray_container());

  // The views::TrayBubbleView ctor will cause a shelf auto hide update check.
  // Make sure to block auto hiding before that check happens.
  should_block_shelf_auto_hide_ = true;

  // Create view, customize it.
  views::TrayBubbleView* bubble_view =
      views::TrayBubbleView::Create(tray_container(), this, &init_params);
  bubble_view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
  bubble_view->UseCompactMargins();
  bubble_view->set_margins(gfx::Insets(0, 0, 0, 0));

  // Add child views.
  auto* title_view = new TitleView(this);
  title_view->SetBorder(views::Border::CreateEmptyBorder(gfx::Insets(
      kPaddingBetweenTitleAndTopEdge, kPaddingBetweenTitleAndLeftEdge,
      kPaddingBetweenTitleAndSeparator, 0)));
  bubble_view->AddChildView(title_view);

  views::Separator* separator = CreateSeparator(views::Separator::HORIZONTAL);
  separator->SetBorder(views::Border::CreateEmptyBorder(
      gfx::Insets(0, 0, kPaddingActionsAndTopSeparator, 0)));
  bubble_view->AddChildView(separator);
  AddToolsToView(bubble_view);

  // Show the bubble.
  bubble_.reset(new ash::TrayBubbleWrapper(this, bubble_view));

  SetDrawBackgroundAsActive(true);
  return true;
}

bool PaletteTray::ContainsPointInScreen(const gfx::Point& point) {
  if (icon_ && icon_->GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

void PaletteTray::AddToolsToView(views::View* host) {
  std::vector<PaletteToolView> views = palette_tool_manager_->CreateViews();
  for (const PaletteToolView& view : views)
    host->AddChildView(view.view);
}

void PaletteTray::SessionStateChanged(
    SessionStateDelegate::SessionState state) {
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

void PaletteTray::BubbleViewDestroyed() {
  palette_tool_manager_->NotifyViewsDestroyed();
  SetDrawBackgroundAsActive(false);
}

void PaletteTray::OnMouseEnteredView() {}

void PaletteTray::OnMouseExitedView() {}

base::string16 PaletteTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect PaletteTray::GetAnchorRect(
    views::Widget* anchor_widget,
    views::TrayBubbleView::AnchorType anchor_type,
    views::TrayBubbleView::AnchorAlignment anchor_alignment) const {
  gfx::Rect r =
      GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);

  // Move the palette to the left so the right edge of the palette aligns with
  // the right edge of the tray button.
  if (IsHorizontalAlignment(shelf_alignment())) {
    // TODO(jdufault): Figure out a more robust adjustment method that does not
    // break in md-shelf.
    int icon_size = tray_container()->width();
    if (tray_container()->border())
      icon_size -= tray_container()->border()->GetInsets().width();

    r.Offset(-r.width() + icon_size, 0);
  } else {
    // Vertical layout doesn't need the border adjustment that horizontal needs.
    r.Offset(0, -r.height() + tray_container()->height());
  }

  return r;
}

void PaletteTray::OnBeforeBubbleWidgetInit(
    views::Widget* anchor_widget,
    views::Widget* bubble_widget,
    views::Widget::InitParams* params) const {
  // Place the bubble in the same root window as |anchor_widget|.
  WmLookup::Get()
      ->GetWindowForWidget(anchor_widget)
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
  SetIconBorderForShelfAlignment();
}

void PaletteTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PaletteTray::SetIconBorderForShelfAlignment() {
  // TODO(tdanderson): Ensure PaletteTray follows material design specs. See
  // crbug.com/630464.
  if (IsHorizontalAlignment(shelf_alignment())) {
    icon_->SetBorder(views::Border::CreateEmptyBorder(gfx::Insets(
        kHorizontalShelfVerticalPadding, kHorizontalShelfHorizontalPadding)));
  } else {
    icon_->SetBorder(views::Border::CreateEmptyBorder(gfx::Insets(
        kVerticalShelfVerticalPadding, kVerticalShelfHorizontalPadding)));
  }
}

void PaletteTray::UpdateTrayIcon() {
  icon_->SetImage(CreateVectorIcon(
      palette_tool_manager_->GetActiveTrayIcon(
          palette_tool_manager_->GetActiveTool(ash::PaletteGroup::MODE)),
      kShelfIconSize, kShelfIconColor));
}

void PaletteTray::OnStylusStateChanged(ui::StylusState stylus_state) {
  PaletteDelegate* palette_delegate = WmShell::Get()->palette_delegate();

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

void PaletteTray::OnPaletteEnabledPrefChanged(bool enabled) {
  if (!enabled) {
    SetVisible(false);
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
  } else {
    UpdateIconVisibility();
  }
}

void PaletteTray::UpdateIconVisibility() {
  SetVisible(IsInUserSession());
}

}  // namespace ash
