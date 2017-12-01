// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/metrics/histogram_macros.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/pointer_watcher.h"

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

// Returns true if the |palette_tray| is on an internal display or on every
// display if requested from the command line.
bool ShouldShowOnDisplay(PaletteTray* palette_tray) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          palette_tray->GetWidget()->GetNativeWindow());
  return display.IsInternal() || stylus_utils::IsPaletteEnabledOnEveryDisplay();
}

class TitleView : public views::View, public views::ButtonListener {
 public:
  explicit TitleView(PaletteTray* palette_tray) : palette_tray_(palette_tray) {
    // TODO(tdanderson|jdufault): Use TriView to handle the layout of the title.
    // See crbug.com/614453.
    auto* box_layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(box_layout);

    auto* title_label =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(title_label);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::TITLE);
    style.SetupLabel(title_label);
    box_layout->SetFlexForView(title_label, 1);
    help_button_ =
        new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                             kSystemMenuHelpIcon, IDS_ASH_STATUS_TRAY_HELP);
    settings_button_ =
        new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                             kSystemMenuSettingsIcon, IDS_ASH_PALETTE_SETTINGS);

    AddChildView(help_button_);
    AddChildView(TrayPopupUtils::CreateVerticalSeparator());
    AddChildView(settings_button_);
  }

  ~TitleView() override = default;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == settings_button_) {
      palette_tray_->RecordPaletteOptionsUsage(
          PaletteTrayOptions::PALETTE_SETTINGS_BUTTON,
          PaletteInvocationMethod::MENU);
      Shell::Get()->system_tray_controller()->ShowPaletteSettings();
      palette_tray_->HidePalette();
    } else if (sender == help_button_) {
      palette_tray_->RecordPaletteOptionsUsage(
          PaletteTrayOptions::PALETTE_HELP_BUTTON,
          PaletteInvocationMethod::MENU);
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

PaletteTray::PaletteTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      palette_tool_manager_(new PaletteToolManager(this)),
      welcome_bubble_(std::make_unique<PaletteWelcomeBubble>(this)),
      scoped_session_observer_(this),
      weak_factory_(this) {
  PaletteTool::RegisterToolInstances(palette_tool_manager_.get());

  SetInkDropMode(InkDropMode::ON);
  SetLayoutManager(new views::FillLayout());
  icon_ = new views::ImageView();
  icon_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE));
  UpdateTrayIcon();

  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  tray_container()->AddChildView(icon_);

  Shell::Get()->AddShellObserver(this);
  ShellPort::Get()->AddPointerWatcher(this,
                                      views::PointerWatcherEventTypes::BASIC);
}

PaletteTray::~PaletteTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();

  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  ShellPort::Get()->RemovePointerWatcher(this);
}

// static
void PaletteTray::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHasSeenStylus, false);
}

// static
void PaletteTray::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kEnableStylusTools, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      prefs::kLaunchPaletteOnEjectEvent, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
}

bool PaletteTray::ContainsPointInScreen(const gfx::Point& point) {
  if (GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

bool PaletteTray::ShouldShowPalette() const {
  return is_palette_enabled_ && stylus_utils::HasStylusInput() &&
         (display::Display::HasInternalDisplay() ||
          stylus_utils::IsPaletteEnabledOnEveryDisplay());
}

void PaletteTray::OnActiveUserPrefServiceChanged(PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  pref_change_registrar_user_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_user_->Init(pref_service);
  pref_change_registrar_user_->Add(
      prefs::kEnableStylusTools,
      base::Bind(&PaletteTray::OnPaletteEnabledPrefChanged,
                 base::Unretained(this)));

  // Read the initial value.
  OnPaletteEnabledPrefChanged();

  // We may need to show the bubble upon switching users for devices with
  // external stylus, but only if the device has seen a stylus before (avoid
  // showing the bubble if the device has never and may never be used with
  // stylus).
  if (HasSeenStylus() && !stylus_utils::HasInternalStylus())
    welcome_bubble_->ShowIfNeeded(false /* shown_by_stylus */);
}

void PaletteTray::OnSessionStateChanged(session_manager::SessionState state) {
  UpdateIconVisibility();
  if (HasSeenStylus() && !stylus_utils::HasInternalStylus())
    welcome_bubble_->ShowIfNeeded(false /* shown_by_stylus */);
}

void PaletteTray::OnLockStateChanged(bool locked) {
  UpdateIconVisibility();

  if (locked) {
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);

    // The user can eject the stylus during the lock screen transition, which
    // will open the palette. Make sure to close it if that happens.
    HidePalette();
  }
}

void PaletteTray::OnLocalStatePrefServiceInitialized(
    PrefService* pref_service) {
  local_state_pref_service_ = pref_service;

  // If a device has an internal stylus or the flag to force stylus is set, mark
  // the has seen stylus flag as true since we know the user has a stylus.
  if (stylus_utils::HasInternalStylus() ||
      stylus_utils::HasForcedStylusInput()) {
    local_state_pref_service_->SetBoolean(prefs::kHasSeenStylus, true);
  }

  pref_change_registrar_local_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_local_->Init(local_state_pref_service_);
  pref_change_registrar_local_->Add(
      prefs::kHasSeenStylus,
      base::Bind(&PaletteTray::OnHasSeenStylusPrefChanged,
                 base::Unretained(this)));

  OnHasSeenStylusPrefChanged();
}

void PaletteTray::ClickedOutsideBubble() {
  if (num_actions_in_bubble_ == 0) {
    RecordPaletteOptionsUsage(PaletteTrayOptions::PALETTE_CLOSED_NO_ACTION,
                              PaletteInvocationMethod::MENU);
  }
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
  if (!stylus_utils::HasStylusInput())
    return;

  // Don't do anything if the palette should not be shown or if the user has
  // disabled it all-together.
  if (!palette_utils::IsInUserSession() || !is_palette_enabled_)
    return;

  // Auto show/hide the palette if allowed by the user.
  if (pref_change_registrar_user_ &&
      pref_change_registrar_user_->prefs()->GetBoolean(
          prefs::kLaunchPaletteOnEjectEvent)) {
    if (stylus_state == ui::StylusState::REMOVED && !bubble_) {
      is_bubble_auto_opened_ = true;
      ShowBubble(false /* show_by_click */);
    } else if (stylus_state == ui::StylusState::INSERTED && bubble_) {
      HidePalette();
    }
  } else if (stylus_state == ui::StylusState::REMOVED) {
    // Show the palette welcome bubble if the auto open palette setting is not
    // turned on, if the bubble has not been shown before (|welcome_bubble_|
    // will be nullptr if the bubble has been shown before).
    welcome_bubble_->ShowIfNeeded(false /* shown_by_stylus */);
  }

  // Disable any active modes if the stylus has been inserted.
  if (stylus_state == ui::StylusState::INSERTED)
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
}

void PaletteTray::BubbleViewDestroyed() {
  palette_tool_manager_->NotifyViewsDestroyed();
  // Opening the palette via an accelerator will close any open widget and then
  // open a new one. This method is called when the widget is closed, but due to
  // async close the new bubble may have already been created. If this happens,
  // |bubble_| will not be null.
  SetIsActive(bubble_ || palette_tool_manager_->GetActiveTool(
                             PaletteGroup::MODE) != PaletteToolId::NONE);
}

void PaletteTray::OnMouseEnteredView() {}

void PaletteTray::OnMouseExitedView() {}

base::string16 PaletteTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool PaletteTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled();
}

void PaletteTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PaletteTray::HidePalette() {
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

void PaletteTray::RecordPaletteOptionsUsage(PaletteTrayOptions option,
                                            PaletteInvocationMethod method) {
  DCHECK_NE(option, PaletteTrayOptions::PALETTE_OPTIONS_COUNT);

  if (method == PaletteInvocationMethod::SHORTCUT) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Shelf.Palette.Usage.Shortcut", option,
                              PaletteTrayOptions::PALETTE_OPTIONS_COUNT);
  } else if (is_bubble_auto_opened_) {
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

void PaletteTray::OnActiveToolChanged() {
  ++num_actions_in_bubble_;

  // If there is no tool currently active and the palette tray button was active
  // (eg. a mode was deactivated without pressing the palette tray button), make
  // the palette tray button inactive.
  if (palette_tool_manager_->GetActiveTool(PaletteGroup::MODE) ==
          PaletteToolId::NONE &&
      is_active()) {
    SetIsActive(false);
  }

  UpdateTrayIcon();
}

aura::Window* PaletteTray::GetWindow() {
  return shelf()->GetWindow();
}

void PaletteTray::OnPointerEventObserved(const ui::PointerEvent& event,
                                         const gfx::Point& location_in_screen,
                                         gfx::NativeView target) {
  if (event.pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN) {
    return;
  }

  // If a stylus has never been seen before and a stylus event is received, mark
  // the |kHasSeenStylus| pref as true and attempt to show the welcome bubble.
  if (!HasSeenStylus()) {
    if (local_state_pref_service_)
      local_state_pref_service_->SetBoolean(prefs::kHasSeenStylus, true);

    if (active_user_pref_service_)
      welcome_bubble_->ShowIfNeeded(true /* shown_by_stylus */);
  } else if (GetBoundsInScreen().Contains(location_in_screen)) {
    // If a stylus event is detected on the palette tray, the user already knows
    // about the tray and there is no need to show them the welcome bubble.
    welcome_bubble_->MarkAsShown();
  }
}

void PaletteTray::AnchorUpdated() {
  if (bubble_) {
    UpdateClippingWindowBounds();
    bubble_->bubble_view()->UpdateBubble();
  }
}

void PaletteTray::Initialize() {
  TrayBackgroundView::Initialize();
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

bool PaletteTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    if (num_actions_in_bubble_ == 0) {
      RecordPaletteOptionsUsage(PaletteTrayOptions::PALETTE_CLOSED_NO_ACTION,
                                PaletteInvocationMethod::MENU);
    }
    HidePalette();
    return true;
  }

  // Do not show the bubble if there was an action on the palette tray while
  // there was an active tool.
  if (DeactivateActiveTool()) {
    SetIsActive(false);
    return true;
  }

  ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
  return true;
}

void PaletteTray::CloseBubble() {
  HidePalette();
}

void PaletteTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  DCHECK(tray_container());

  // There may still be an active tool if show bubble was called from an
  // accelerator.
  DeactivateActiveTool();

  views::TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = GetBubbleAnchor();
  init_params.anchor_alignment = GetAnchorAlignment();
  init_params.min_width = kPaletteWidth;
  init_params.max_width = kPaletteWidth;
  init_params.close_on_deactivate = true;
  init_params.show_by_click = show_by_click;

  // TODO(tdanderson): Refactor into common row layout code.
  // TODO(tdanderson|jdufault): Add material design ripple effects to the menu
  // rows.

  // Create and customize bubble view.
  views::TrayBubbleView* bubble_view = new views::TrayBubbleView(init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());
  bubble_view->set_margins(
      gfx::Insets(kPalettePaddingOnTop, 0, kPalettePaddingOnBottom, 0));

  // Add title.
  auto* title_view = new TitleView(this);
  title_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kPaddingBetweenTitleAndLeftEdge, 0, 0)));
  bubble_view->AddChildView(title_view);

  // Add horizontal separator between the title and tools.
  auto* separator = new views::Separator();
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
  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view,
                                                false /* is_persistent */);
  SetIsActive(true);
}

views::TrayBubbleView* PaletteTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

void PaletteTray::UpdateTrayIcon() {
  icon_->SetImage(CreateVectorIcon(
      palette_tool_manager_->GetActiveTrayIcon(
          palette_tool_manager_->GetActiveTool(PaletteGroup::MODE)),
      kTrayIconSize, kShelfIconColor));
}

void PaletteTray::OnPaletteEnabledPrefChanged() {
  is_palette_enabled_ = pref_change_registrar_user_->prefs()->GetBoolean(
      prefs::kEnableStylusTools);

  if (!is_palette_enabled_) {
    SetVisible(false);
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
  } else {
    UpdateIconVisibility();
  }
}

void PaletteTray::OnHasSeenStylusPrefChanged() {
  DCHECK(local_state_pref_service_);

  UpdateIconVisibility();
}

bool PaletteTray::DeactivateActiveTool() {
  PaletteToolId active_tool_id =
      palette_tool_manager_->GetActiveTool(PaletteGroup::MODE);
  if (active_tool_id != PaletteToolId::NONE) {
    palette_tool_manager_->DeactivateTool(active_tool_id);
    // TODO(sammiequon): Investigate whether we should removed |is_switched|
    // from PaletteToolIdToPaletteModeCancelType.
    RecordPaletteModeCancellation(PaletteToolIdToPaletteModeCancelType(
        active_tool_id, false /*is_switched*/));
    return true;
  }

  return false;
}

bool PaletteTray::HasSeenStylus() {
  return local_state_pref_service_ &&
         local_state_pref_service_->GetBoolean(prefs::kHasSeenStylus);
}

void PaletteTray::UpdateIconVisibility() {
  SetVisible(HasSeenStylus() && is_palette_enabled_ &&
             stylus_utils::HasStylusInput() && ShouldShowOnDisplay(this) &&
             palette_utils::IsInUserSession());
}

}  // namespace ash
