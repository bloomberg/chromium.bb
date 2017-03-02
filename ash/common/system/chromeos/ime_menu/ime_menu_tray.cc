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
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

using chromeos::input_method::InputMethodManager;

namespace ash {

namespace {
// Returns the height range of ImeListView.
gfx::Range GetImeListViewRange() {
  const int max_items = 5;
  const int min_items = 1;
  const int tray_item_height = kTrayPopupItemMinHeight;
  return gfx::Range(tray_item_height * min_items, tray_item_height * max_items);
}

// Returns the minimum with of IME menu.
int GetMinimumMenuWidth() {
  return MaterialDesignController::IsSystemTrayMenuMaterial()
             ? kTrayMenuMinimumWidthMd
             : kTrayMenuMinimumWidth;
}

// Shows language and input settings page.
void ShowIMESettings() {
  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SHOW_DETAILED);
  WmShell::Get()->system_tray_controller()->ShowIMESettings();
}

// Records the number of times users click buttons in opt-in IME menu.
void RecordButtonsClicked(const std::string& button_name) {
  enum {
    UNKNOWN = 0,
    EMOJI = 1,
    HANDWRITING = 2,
    VOICE = 3,
    // SETTINGS is not used for now.
    SETTINGS = 4,
    BUTTON_MAX
  } button = UNKNOWN;
  if (button_name == "emoji") {
    button = EMOJI;
  } else if (button_name == "hwt") {
    button = HANDWRITING;
  } else if (button_name == "voice") {
    button = VOICE;
  }
  UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeMenu.EmojiHandwritingVoiceButton",
                            button, BUTTON_MAX);
}

// Returns true if the current screen is login or lock screen.
bool IsInLoginOrLockScreen() {
  LoginStatus login =
      WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
  return !TrayPopupUtils::CanOpenWebUISettings(login);
}

// Returns true if the current input context type is password.
bool IsInPasswordInputContext() {
  return ui::IMEBridge::Get()->GetCurrentInputContext().type ==
         ui::TEXT_INPUT_TYPE_PASSWORD;
}

class ImeMenuLabel : public views::Label {
 public:
  ImeMenuLabel() {}
  ~ImeMenuLabel() override {}

  // views:Label:
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(kTrayItemSize, kTrayItemSize);
  }
  int GetHeightForWidth(int width) const override { return kTrayItemSize; }

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
                                      kMenuSeparatorColor),
        gfx::Insets(kMenuSeparatorVerticalPadding - kSeparatorWidth, 0)));
    auto* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    box_layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
    SetLayoutManager(box_layout);
    auto* title_label =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
    title_label->SetBorder(
        views::CreateEmptyBorder(0, kMenuEdgeEffectivePadding, 1, 0));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::TITLE);
    style.SetupLabel(title_label);

    AddChildView(title_label);
    box_layout->SetFlexForView(title_label, 1);

    if (show_settings_button) {
      settings_button_ = CreateImeMenuButton(
          this, kSystemMenuSettingsIcon, IDS_ASH_STATUS_TRAY_IME_SETTINGS, 0);
      if (IsInLoginOrLockScreen())
        settings_button_->SetEnabled(false);
      AddChildView(settings_button_);
    }
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, settings_button_);
    ShowIMESettings();
  }

  ~ImeTitleView() override {}

 private:
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
    if (sender == emoji_button_) {
      keyset = "emoji";
      RecordButtonsClicked(keyset);
    } else if (sender == voice_button_) {
      keyset = "voice";
      RecordButtonsClicked(keyset);
    } else if (sender == handwriting_button_) {
      keyset = "hwt";
      RecordButtonsClicked(keyset);
    } else {
      NOTREACHED();
    }

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
      one_settings_button_view_->SetEnabled(false);
    AddChildView(one_settings_button_view_);
  }

  // Shows the UI of more than one buttons.
  void ShowButtons(bool show_emoji_button,
                   bool show_handwriting_button,
                   bool show_voice_button,
                   bool show_settings_button) {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      auto* box_layout =
          new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
      box_layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
      SetLayoutManager(box_layout);
      SetBorder(views::CreatePaddedBorder(
          views::CreateSolidSidedBorder(kSeparatorWidth, 0, 0, 0,
                                        kMenuSeparatorColor),
          gfx::Insets(kMenuSeparatorVerticalPadding - kSeparatorWidth,
                      kMenuExtraMarginFromLeftEdge)));
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

// The list view that contains the selected IME and property items.
class ImeMenuListView : public ImeListView {
 public:
  ImeMenuListView(SystemTrayItem* owner) : ImeListView(owner) {
    set_should_focus_ime_after_selection_with_keyboard(true);
  }

  ~ImeMenuListView() override {}

 protected:
  void Layout() override {
    gfx::Range height_range = GetImeListViewRange();
    scroller()->ClipHeightTo(height_range.start(), height_range.end());
    ImeListView::Layout();
  }

  DISALLOW_COPY_AND_ASSIGN(ImeMenuListView);
};

}  // namespace

ImeMenuTray::ImeMenuTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      label_(new ImeMenuLabel()),
      show_keyboard_(false),
      force_show_keyboard_(false),
      should_block_shelf_auto_hide_(false),
      keyboard_suppressed_(false),
      show_bubble_after_keyboard_hidden_(false) {
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
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);
}

void ImeMenuTray::ShowImeMenuBubble() {
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller->keyboard_visible()) {
    show_bubble_after_keyboard_hidden_ = true;
    keyboard_controller->AddObserver(this);
    keyboard_controller->HideKeyboard(
        keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  } else {
    ShowImeMenuBubbleInternal();
  }
}

void ImeMenuTray::ShowImeMenuBubbleInternal() {
  int minimum_menu_width = GetMinimumMenuWidth();
  should_block_shelf_auto_hide_ = true;
  views::TrayBubbleView::InitParams init_params(
      GetAnchorAlignment(), minimum_menu_width, minimum_menu_width);
  init_params.can_activate = true;
  init_params.close_on_deactivate = true;

  views::TrayBubbleView* bubble_view =
      views::TrayBubbleView::Create(GetBubbleAnchor(), this, &init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());

  // In the material design, we will add a title item with a separator on the
  // top of the IME menu.
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    bubble_view->AddChildView(
        new ImeTitleView(!ShouldShowEmojiHandwritingVoiceButtons()));
  } else {
    bubble_view->set_margins(gfx::Insets(7, 0, 0, 0));
  }

  // Adds IME list to the bubble.
  ime_list_view_ = new ImeMenuListView(nullptr);
  ime_list_view_->Init(ShouldShowKeyboardToggle(),
                       ImeListView::SHOW_SINGLE_IME);
  bubble_view->AddChildView(ime_list_view_);

  if (ShouldShowEmojiHandwritingVoiceButtons()) {
    bubble_view->AddChildView(new ImeButtonsView(this, true, true, true, true));
  } else if (!MaterialDesignController::IsSystemTrayMenuMaterial()) {
    // For MD, we don't need |ImeButtonsView| as the settings button will be
    // shown in the title row.
    bubble_view->AddChildView(
        new ImeButtonsView(this, false, false, false, true));
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
    // If the keyboard window hasn't been created yet, it means the extension
    // cannot receive anything to show the keyboard. Therefore, instead of
    // relying the extension to show the keyboard, forcibly show the keyboard
    // window here (which will cause the keyboard window to be created).
    // Otherwise, the extension will show keyboard by calling private api. The
    // native side could just skip showing the keyboard.
    if (!keyboard_controller->IsKeyboardWindowCreated())
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
  // Emoji, handwriting and voice input is not supported for these cases:
  // 1) features::kEHVInputOnImeMenu is not enabled.
  // 2) third party IME extensions.
  // 3) login/lock screen.
  // 4) password input client.
  return InputMethodManager::Get() &&
         InputMethodManager::Get()->IsEmojiHandwritingVoiceOnImeMenuEnabled() &&
         !current_ime_.third_party && !IsInLoginOrLockScreen() &&
         !IsInPasswordInputContext();
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
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);

  show_keyboard_ = false;
  force_show_keyboard_ = false;
}

void ImeMenuTray::OnKeyboardHidden() {
  if (show_bubble_after_keyboard_hidden_) {
    show_bubble_after_keyboard_hidden_ = false;
    keyboard::KeyboardController* keyboard_controller =
        keyboard::KeyboardController::GetInstance();
    if (keyboard_controller)
      keyboard_controller->RemoveObserver(this);

    ShowImeMenuBubbleInternal();
    return;
  }

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
