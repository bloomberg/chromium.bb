// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accessibility_delegate.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/metrics/histogram_macros.h"
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

// Returns true if we are in a user session that can show the stylus tools.
bool IsInUserSession() {
  SessionController* session_controller = Shell::Get()->session_controller();
  return !session_controller->IsUserSessionBlocked() &&
         session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !session_controller->IsKioskSession();
}

// Returns true if the |palette_tray| is on an internal display or on every
// display if requested from the command line.
bool ShouldShowOnDisplay(PaletteTray* palette_tray) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          palette_tray->GetWidget()->GetNativeWindow());
  return display.IsInternal() ||
         palette_utils::IsPaletteEnabledOnEveryDisplay();
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

  ~TitleView() override {}

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

// StylusWatcher is used to monitor for stylus events, since we only want to
// make the palette tray visible for devices without internal styluses once they
// start using the stylus.
class PaletteTray::StylusWatcher : views::PointerWatcher {
 public:
  explicit StylusWatcher(PrefService* pref_service)
      : local_state_pref_service_(pref_service) {
    ShellPort::Get()->AddPointerWatcher(this,
                                        views::PointerWatcherEventTypes::BASIC);
  }

  ~StylusWatcher() override { ShellPort::Get()->RemovePointerWatcher(this); }

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              gfx::NativeView target) override {
    if (event.pointer_details().pointer_type ==
        ui::EventPointerType::POINTER_TYPE_PEN) {
      if (local_state_pref_service_)
        local_state_pref_service_->SetBoolean(prefs::kHasSeenStylus, true);
    }
  }

 private:
  PrefService* local_state_pref_service_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(StylusWatcher);
};

PaletteTray::PaletteTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      palette_tool_manager_(new PaletteToolManager(this)),
      scoped_session_observer_(this),
      weak_factory_(this) {
  PaletteTool::RegisterToolInstances(palette_tool_manager_.get());

  SetInkDropMode(InkDropMode::ON);
  SetLayoutManager(new views::FillLayout());
  icon_ = new views::ImageView();
  UpdateTrayIcon();

  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  tray_container()->AddChildView(icon_);

  Shell::Get()->AddShellObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);

  if (!drag_controller())
    set_drag_controller(base::MakeUnique<TrayDragController>(shelf));
}

PaletteTray::~PaletteTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();

  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

// static
void PaletteTray::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHasSeenStylus, false);
}

bool PaletteTray::ContainsPointInScreen(const gfx::Point& point) {
  if (icon_ && icon_->GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

void PaletteTray::OnSessionStateChanged(session_manager::SessionState state) {
  UpdateIconVisibility();
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

  // May be null in mash_unittests where there is no mojo pref service.
  if (!local_state_pref_service_)
    return;

  // If a device has an internal stylus or the flag to force stylus is set, mark
  // the has seen stylus flag as true since we know the user has a stylus.
  if (palette_utils::HasInternalStylus() ||
      palette_utils::HasForcedStylusInput()) {
    local_state_pref_service_->SetBoolean(prefs::kHasSeenStylus, true);
  }

  pref_change_registrar_ = base::MakeUnique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state_pref_service_);
  pref_change_registrar_->Add(
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
  if (!palette_utils::HasStylusInput())
    return;

  PaletteDelegate* palette_delegate = Shell::Get()->palette_delegate();

  // Don't do anything if the palette should not be shown or if the user has
  // disabled it all-together.
  if (!IsInUserSession() || !palette_delegate->ShouldShowPalette())
    return;

  // Auto show/hide the palette if allowed by the user.
  if (palette_delegate->ShouldAutoOpenPalette()) {
    if (stylus_state == ui::StylusState::REMOVED && !bubble_) {
      is_bubble_auto_opened_ = true;
      ShowBubble();
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
  // The tray button remains active if the current active tool is a mode.
  SetIsActive(palette_tool_manager_->GetActiveTool(PaletteGroup::MODE) !=
              PaletteToolId::NONE);
}

void PaletteTray::OnMouseEnteredView() {}

void PaletteTray::OnMouseExitedView() {}

void PaletteTray::RegisterAccelerators(
    const std::vector<ui::Accelerator>& accelerators,
    views::TrayBubbleView* tray_bubble_view) {
  Shell::Get()->accelerator_controller()->Register(accelerators,
                                                   tray_bubble_view);
}

void PaletteTray::UnregisterAllAccelerators(
    views::TrayBubbleView* tray_bubble_view) {
  Shell::Get()->accelerator_controller()->UnregisterAll(tray_bubble_view);
}

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

void PaletteTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PaletteTray::Initialize() {
  PaletteDelegate* delegate = Shell::Get()->palette_delegate();
  // |delegate| can be null in tests.
  if (!delegate)
    return;

  // OnPaletteEnabledPrefChanged will get called with the initial pref value,
  // which will take care of showing the palette.
  palette_enabled_subscription_ = delegate->AddPaletteEnableListener(base::Bind(
      &PaletteTray::OnPaletteEnabledPrefChanged, weak_factory_.GetWeakPtr()));
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

  // Deactivate the active tool if there is one.
  PaletteToolId active_tool_id =
      palette_tool_manager_->GetActiveTool(PaletteGroup::MODE);
  if (active_tool_id != PaletteToolId::NONE) {
    palette_tool_manager_->DeactivateTool(active_tool_id);
    // TODO(sammiequon): Investigate whether we should removed |is_switched|
    // from PaletteToolIdToPaletteModeCancelType.
    RecordPaletteModeCancellation(PaletteToolIdToPaletteModeCancelType(
        active_tool_id, false /*is_switched*/));
    SetIsActive(false);
    return true;
  }

  ShowBubble();
  return true;
}

void PaletteTray::CloseBubble() {
  HidePalette();
}

void PaletteTray::ShowBubble() {
  if (bubble_)
    return;

  DCHECK(tray_container());

  views::TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = GetBubbleAnchor();
  init_params.anchor_alignment = GetAnchorAlignment();
  init_params.min_width = kPaletteWidth;
  init_params.max_width = kPaletteWidth;
  init_params.close_on_deactivate = true;

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
  bubble_ = base::MakeUnique<ash::TrayBubbleWrapper>(this, bubble_view);
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

void PaletteTray::OnPaletteEnabledPrefChanged(bool enabled) {
  is_palette_enabled_ = enabled;

  if (!enabled) {
    SetVisible(false);
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
  } else {
    UpdateIconVisibility();
  }
}

void PaletteTray::OnHasSeenStylusPrefChanged() {
  DCHECK(local_state_pref_service_);

  has_seen_stylus_ =
      local_state_pref_service_->GetBoolean(prefs::kHasSeenStylus);

  // On reading the pref, do not bother monitoring stylus events if the device
  // has seen a stylus event before, otherwise start monitoring for stylus
  // events.
  // TODO(sammiequon): Investigate if we can avoid starting the watcher if the
  // device is not compatible with stylus.
  if (has_seen_stylus_)
    watcher_.reset();
  else
    watcher_ = base::MakeUnique<StylusWatcher>(local_state_pref_service_);

  UpdateIconVisibility();
}

void PaletteTray::UpdateIconVisibility() {
  SetVisible(has_seen_stylus_ && is_palette_enabled_ &&
             palette_utils::HasStylusInput() && ShouldShowOnDisplay(this) &&
             IsInUserSession());
}

// TestApi. For testing purposes.
PaletteTray::TestApi::TestApi(PaletteTray* palette_tray)
    : palette_tray_(palette_tray) {
  DCHECK(palette_tray_);
}

PaletteTray::TestApi::~TestApi() {}

}  // namespace ash
