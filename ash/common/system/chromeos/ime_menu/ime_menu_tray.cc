// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

using chromeos::input_method::InputMethodManager;

namespace ash {

namespace {
// Returns the height range of ImeListView.
gfx::Range GetImeListViewRange() {
  const int max_items = 5;
  const int min_items = 2;
  const int tray_item_height = GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT);
  return gfx::Range(tray_item_height * min_items, tray_item_height * max_items);
}

// Shows language and input settings page.
void ShowIMESettings() {
  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SHOW_DETAILED);
  WmShell::Get()->system_tray_controller()->ShowIMESettings();
}

// Returns true if the current screen is login or lock screen.
bool IsInLoginOrLockScreen() {
  LoginStatus login =
      WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
  return !TrayPopupUtils::CanOpenWebUISettings(login);
}

class ImeMenuLabel : public views::Label {
 public:
  ImeMenuLabel() {}
  ~ImeMenuLabel() override {}

  // views:Label:
  gfx::Size GetPreferredSize() const override {
    const int tray_constant = GetTrayConstant(TRAY_IME_MENU_ICON);
    return gfx::Size(tray_constant, tray_constant);
  }
  int GetHeightForWidth(int width) const override {
    return GetTrayConstant(TRAY_IME_MENU_ICON);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeMenuLabel);
};

SystemMenuButton* CreateImeMenuButton(views::ButtonListener* listener,
                                      const gfx::VectorIcon& icon,
                                      int accessible_name_id,
                                      int right_border) {
  SystemMenuButton* button = new SystemMenuButton(
      listener, TrayPopupInkDropStyle::HOST_CENTERED, icon, accessible_name_id);
  if (!MaterialDesignController::IsShelfMaterial()) {
    button->SetBorder(
        views::CreateSolidSidedBorder(0, 0, 0, right_border, kBorderDarkColor));
  }
  return button;
}

// The view that contains IME menu title in the material design.
class ImeTitleView : public views::View, public views::ButtonListener {
 public:
  explicit ImeTitleView(bool show_settings_button) : settings_button_(nullptr) {
    SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(0, 0, kSeparatorWidth, 0,
                                      kHorizontalSeparatorColor),
        gfx::Insets(kMenuSeparatorVerticalPadding - kSeparatorWidth, 0)));
    auto* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    box_layout->set_minimum_cross_axis_size(
        GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT));
    SetLayoutManager(box_layout);
    title_label_ =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
    title_label_->SetBorder(
        views::CreateEmptyBorder(0, kMenuEdgeEffectivePadding, 0, 0));
    title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(title_label_);
    box_layout->SetFlexForView(title_label_, 1);

    if (show_settings_button) {
      settings_button_ = CreateImeMenuButton(
          this, kSystemMenuSettingsIcon, IDS_ASH_STATUS_TRAY_IME_SETTINGS, 0);
      if (IsInLoginOrLockScreen())
        settings_button_->SetState(views::Button::STATE_DISABLED);
      AddChildView(settings_button_);
    }
  }

  // views::View:
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    TrayPopupItemStyle style(theme, TrayPopupItemStyle::FontStyle::TITLE);
    style.SetupLabel(title_label_);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, settings_button_);
    ShowIMESettings();
  }

  ~ImeTitleView() override {}

 private:
  views::Label* title_label_;

  // Settings button that is only used in material design, and only if the
  // emoji, handwriting and voice buttons are not available.
  SystemMenuButton* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(ImeTitleView);
};

// The view that contains buttons shown on the bottom of IME menu.
class ImeButtonsView : public views::View,
                       public views::ButtonListener,
                       public ViewClickListener {
 public:
  ImeButtonsView(ImeMenuTray* ime_menu_tray,
                 bool show_emoji_button,
                 bool show_voice_button,
                 bool show_handwriting_button,
                 bool show_settings_button)
      : ime_menu_tray_(ime_menu_tray) {
    DCHECK(ime_menu_tray_);

    if (!MaterialDesignController::IsSystemTrayMenuMaterial())
      SetBorder(views::CreateSolidSidedBorder(1, 0, 0, 0, kBorderDarkColor));

    // If there's only one settings button, the bottom should be a label with
    // normal background. Otherwise, show button icons with header background.
    if (show_settings_button && !show_emoji_button &&
        !show_handwriting_button && !show_voice_button) {
      DCHECK(!MaterialDesignController::IsSystemTrayMenuMaterial());
      ShowOneSettingButton();
    } else {
      ShowButtons(show_emoji_button, show_handwriting_button, show_voice_button,
                  show_settings_button);
    }
  }

  ~ImeButtonsView() override {}

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == settings_button_) {
      ime_menu_tray_->HideImeMenuBubble();
      ShowIMESettings();
      return;
    }

    // The |keyset| will be used for drawing input view keyset in IME
    // extensions. InputMethodManager::ShowKeyboardWithKeyset() will deal with
    // the |keyset| string to generate the right input view url.
    std::string keyset;
    if (sender == emoji_button_)
      keyset = "emoji";
    else if (sender == voice_button_)
      keyset = "voice";
    else if (sender == handwriting_button_)
      keyset = "hwt";
    else
      NOTREACHED();

    ime_menu_tray_->ShowKeyboardWithKeyset(keyset);
  }

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override {
    if (one_settings_button_view_ && sender == one_settings_button_view_) {
      ime_menu_tray_->HideImeMenuBubble();
      ShowIMESettings();
    }
  }

 private:
  // Shows the UI of one settings button.
  void ShowOneSettingButton() {
    auto* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    box_layout->SetDefaultFlex(1);
    SetLayoutManager(box_layout);
    one_settings_button_view_ = new HoverHighlightView(this);
    one_settings_button_view_->AddLabel(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_IME_SETTINGS),
        gfx::ALIGN_LEFT, false /* highlight */);
    if (IsInLoginOrLockScreen())
      one_settings_button_view_->SetState(views::Button::STATE_DISABLED);
    AddChildView(one_settings_button_view_);
  }

  // Shows the UI of more than one buttons.
  void ShowButtons(bool show_emoji_button,
                   bool show_handwriting_button,
                   bool show_voice_button,
                   bool show_settings_button) {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      SetLayoutManager(
          new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
      SetBorder(views::CreatePaddedBorder(
          views::CreateSolidSidedBorder(kSeparatorWidth, 0, 0, 0,
                                        kHorizontalSeparatorColor),
          gfx::Insets(kMenuSeparatorVerticalPadding - kSeparatorWidth,
                      GetTrayConstant(TRAY_POPUP_ITEM_LEFT_INSET))));
    } else {
      auto* box_layout =
          new views::BoxLayout(views::BoxLayout::kHorizontal, 4, 4, 0);
      set_background(
          views::Background::CreateSolidBackground(kHeaderBackgroundColor));
      box_layout->SetDefaultFlex(1);
      SetLayoutManager(box_layout);
    }

    const int right_border = 1;
    if (show_emoji_button) {
      emoji_button_ =
          CreateImeMenuButton(this, kImeMenuEmoticonIcon,
                              IDS_ASH_STATUS_TRAY_IME_EMOJI, right_border);
      AddChildView(emoji_button_);
    }

    if (show_handwriting_button) {
      handwriting_button_ = CreateImeMenuButton(
          this, kImeMenuWriteIcon, IDS_ASH_STATUS_TRAY_IME_HANDWRITING,
          right_border);
      AddChildView(handwriting_button_);
    }

    if (show_voice_button) {
      voice_button_ =
          CreateImeMenuButton(this, kImeMenuMicrophoneIcon,
                              IDS_ASH_STATUS_TRAY_IME_VOICE, right_border);
      AddChildView(voice_button_);
    }

    if (show_settings_button) {
      settings_button_ = CreateImeMenuButton(
          this, kSystemMenuSettingsIcon, IDS_ASH_STATUS_TRAY_IME_SETTINGS, 0);
      AddChildView(settings_button_);
    }
  }

  ImeMenuTray* ime_menu_tray_;
  SystemMenuButton* emoji_button_;
  SystemMenuButton* handwriting_button_;
  SystemMenuButton* voice_button_;
  SystemMenuButton* settings_button_;
  HoverHighlightView* one_settings_button_view_;

  DISALLOW_COPY_AND_ASSIGN(ImeButtonsView);
};

}  // namespace

ImeMenuTray::ImeMenuTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      label_(new ImeMenuLabel()),
      show_keyboard_(false),
      force_show_keyboard_(false),
      should_block_shelf_auto_hide_(false),
      keyboard_suppressed_(false) {
  if (MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON);
    SetContentsBackground(false);
  } else {
    SetContentsBackground(true);
  }
  SetupLabelForTray(label_);
  tray_container()->AddChildView(label_);
  SystemTrayNotifier* tray_notifier = WmShell::Get()->system_tray_notifier();
  tray_notifier->AddIMEObserver(this);
  tray_notifier->AddVirtualKeyboardObserver(this);
}

ImeMenuTray::~ImeMenuTray() {
  if (bubble_)
    bubble_->bubble_view()->reset_delegate();
  SystemTrayNotifier* tray_notifier = WmShell::Get()->system_tray_notifier();
  tray_notifier->RemoveIMEObserver(this);
  tray_notifier->RemoveVirtualKeyboardObserver(this);
}

void ImeMenuTray::ShowImeMenuBubble() {
  should_block_shelf_auto_hide_ = true;
  views::TrayBubbleView::InitParams init_params(
      GetAnchorAlignment(), kTrayPopupMinWidth, kTrayPopupMaxWidth);
  init_params.can_activate = true;
  init_params.close_on_deactivate = true;

  views::TrayBubbleView* bubble_view =
      views::TrayBubbleView::Create(GetBubbleAnchor(), this, &init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());

  // In the material design, we will add a title item with a separator on the
  // top of the IME menu.
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    bubble_view->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
    bubble_view->AddChildView(
        new ImeTitleView(!ShouldShowEmojiHandwritingVoiceButtons()));
  } else {
    bubble_view->set_margins(gfx::Insets(7, 0, 0, 0));
  }

  // Adds IME list to the bubble.
  ime_list_view_ = new ImeListView(nullptr, ShouldShowKeyboardToggle(),
                                   ImeListView::SHOW_SINGLE_IME);

  uint32_t current_height = ime_list_view_->scroll_content()->height();
  const gfx::Range height_range = GetImeListViewRange();

  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    ime_list_view_->scroller()->ClipHeightTo(height_range.start(),
                                             height_range.end());
  } else if (current_height > height_range.end()) {
    ime_list_view_->scroller()->SetFixedSize(
        gfx::Size(kTrayPopupMaxWidth, height_range.end()));
  } else if (current_height < height_range.start()) {
    ime_list_view_->scroller()->SetFixedSize(
        gfx::Size(kTrayPopupMaxWidth, height_range.start()));
  }
  bubble_view->AddChildView(ime_list_view_);

  // The bottom view that contains buttons are not supported in login/lock
  // screen.
  if (!IsInLoginOrLockScreen()) {
    if (ShouldShowEmojiHandwritingVoiceButtons()) {
      bubble_view->AddChildView(
          new ImeButtonsView(this, true, true, true, true));
    } else if (!MaterialDesignController::IsSystemTrayMenuMaterial()) {
      // For MD, we don't need |ImeButtonsView| as the settings button will be
      // shown in the title row.
      bubble_view->AddChildView(
          new ImeButtonsView(this, false, false, false, true));
    }
  }

  bubble_.reset(new TrayBubbleWrapper(this, bubble_view));
  SetIsActive(true);
}

void ImeMenuTray::HideImeMenuBubble() {
  bubble_.reset();
  ime_list_view_ = nullptr;
  SetIsActive(false);
  should_block_shelf_auto_hide_ = false;
  shelf()->UpdateAutoHideState();
}

bool ImeMenuTray::IsImeMenuBubbleShown() {
  return !!bubble_;
}

void ImeMenuTray::ShowKeyboardWithKeyset(const std::string& keyset) {
  HideImeMenuBubble();

  // Overrides the keyboard url ref to make it shown with the given keyset.
  if (InputMethodManager::Get())
    InputMethodManager::Get()->OverrideKeyboardUrlRef(keyset);

  // If onscreen keyboard has been enabled, shows the keyboard directly.
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  show_keyboard_ = true;
  if (keyboard_controller) {
    keyboard_controller->AddObserver(this);
    keyboard_controller->ShowKeyboard(false);
    return;
  }

  AccessibilityDelegate* accessibility_delegate =
      WmShell::Get()->accessibility_delegate();
  // Fails to show the keyboard.
  if (accessibility_delegate->IsVirtualKeyboardEnabled())
    return;

  // Onscreen keyboard has not been enabled yet, forces to bring out the
  // keyboard for one time.
  force_show_keyboard_ = true;
  accessibility_delegate->SetVirtualKeyboardEnabled(true);
  keyboard_controller = keyboard::KeyboardController::GetInstance();
  if (keyboard_controller) {
    keyboard_controller->AddObserver(this);
    keyboard_controller->ShowKeyboard(false);
  }
}

bool ImeMenuTray::ShouldBlockShelfAutoHide() const {
  return should_block_shelf_auto_hide_;
}

bool ImeMenuTray::ShouldShowEmojiHandwritingVoiceButtons() const {
  return InputMethodManager::Get() &&
         InputMethodManager::Get()->IsEmojiHandwritingVoiceOnImeMenuEnabled() &&
         !current_ime_.third_party;
}

bool ImeMenuTray::ShouldShowKeyboardToggle() const {
  return keyboard_suppressed_ &&
         !WmShell::Get()->accessibility_delegate()->IsVirtualKeyboardEnabled();
}

void ImeMenuTray::SetShelfAlignment(ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  if (!MaterialDesignController::IsShelfMaterial())
    tray_container()->SetBorder(views::NullBorder());
}

base::string16 ImeMenuTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

void ImeMenuTray::HideBubbleWithView(const views::TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HideImeMenuBubble();
}

void ImeMenuTray::ClickedOutsideBubble() {
  HideImeMenuBubble();
}

bool ImeMenuTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    HideImeMenuBubble();
  else
    ShowImeMenuBubble();
  return true;
}

void ImeMenuTray::OnIMERefresh() {
  UpdateTrayLabel();
  if (bubble_ && ime_list_view_) {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    IMEInfoList list;
    delegate->GetAvailableIMEList(&list);
    IMEPropertyInfoList property_list;
    delegate->GetCurrentIMEProperties(&property_list);
    ime_list_view_->Update(list, property_list, false,
                           ImeListView::SHOW_SINGLE_IME);
  }
}

void ImeMenuTray::OnIMEMenuActivationChanged(bool is_activated) {
  SetVisible(is_activated);
  if (is_activated)
    UpdateTrayLabel();
  else
    HideImeMenuBubble();
}

void ImeMenuTray::BubbleViewDestroyed() {
}

void ImeMenuTray::OnMouseEnteredView() {}

void ImeMenuTray::OnMouseExitedView() {}

base::string16 ImeMenuTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

void ImeMenuTray::OnBeforeBubbleWidgetInit(
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

void ImeMenuTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void ImeMenuTray::OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) {}

void ImeMenuTray::OnKeyboardClosed() {
  if (InputMethodManager::Get())
    InputMethodManager::Get()->OverrideKeyboardUrlRef(std::string());
  show_keyboard_ = false;
  force_show_keyboard_ = false;
}

void ImeMenuTray::OnKeyboardHidden() {
  if (!show_keyboard_)
    return;

  // If the the IME menu has overriding the input view url, we should write it
  // back to normal keyboard when hiding the input view.
  if (InputMethodManager::Get())
    InputMethodManager::Get()->OverrideKeyboardUrlRef(std::string());
  show_keyboard_ = false;

  // If the keyboard is forced to be shown by IME menu for once, we need to
  // disable the keyboard when it's hidden.
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);

  if (!force_show_keyboard_)
    return;

  WmShell::Get()->accessibility_delegate()->SetVirtualKeyboardEnabled(false);
  force_show_keyboard_ = false;
}

void ImeMenuTray::OnKeyboardSuppressionChanged(bool suppressed) {
  if (suppressed != keyboard_suppressed_ && bubble_)
    HideImeMenuBubble();
  keyboard_suppressed_ = suppressed;
}

void ImeMenuTray::UpdateTrayLabel() {
  WmShell::Get()->system_tray_delegate()->GetCurrentIME(&current_ime_);

  // Updates the tray label based on the current input method.
  if (current_ime_.third_party)
    label_->SetText(current_ime_.short_name + base::UTF8ToUTF16("*"));
  else
    label_->SetText(current_ime_.short_name);
}

}  // namespace ash
