// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/status/network_dropdown_button.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/layout/fill_layout.h"
#include "views/widget/widget.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

namespace {

// JS API callbacks names.
const char kJsApiNetworkOnExit[] = "networkOnExit";
const char kJsApiNetworkControlPosition[] = "networkControlPosition";

// Width/height of the network control window.
const int kNetworkControlWidth = 150;
const int kNetworkControlHeight = 25;

// Offsets for the network dropdown control menu.
const int kMenuHorizontalOffset = -3;
const int kMenuVerticalOffset = -1;

// Initializes menu button default properties.
static void InitMenuButtonProperties(views::MenuButton* menu_button) {
  menu_button->set_focusable(true);
  menu_button->SetEnabledColor(SK_ColorBLACK);
  menu_button->SetHighlightColor(SK_ColorBLACK);
  menu_button->SetHoverColor(SK_ColorBLACK);
  static_cast<views::TextButtonBorder*>(menu_button->border())->
      copy_normal_set_to_hot_set();
  menu_button->set_animate_on_state_change(false);
  // Menu is positioned by bottom right corner of the MenuButton.
  menu_button->set_menu_offset(kMenuHorizontalOffset, kMenuVerticalOffset);
}

}  // namespace

namespace chromeos {

// NetworkScreenHandler, public: -----------------------------------------------

NetworkScreenHandler::NetworkScreenHandler()
    : network_window_(NULL),
      screen_(NULL),
      is_continue_enabled_(false),
      show_on_init_(false) {
}

NetworkScreenHandler::~NetworkScreenHandler() {
  ClearErrors();
  CloseNetworkWindow();
}

// NetworkScreenHandler, NetworkScreenActor implementation: --------------------

void NetworkScreenHandler::SetDelegate(NetworkScreenActor::Delegate* screen) {
  screen_ = screen;
}

void NetworkScreenHandler::PrepareToShow() {
}

void NetworkScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  scoped_ptr<Value> value(Value::CreateIntegerValue(0));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.toggleStep", *value);
}

void NetworkScreenHandler::Hide() {
  CloseNetworkWindow();
}

void NetworkScreenHandler::ShowError(const string16& message) {
  // scoped_ptr<Value> message_value(Value::CreateStringValue(message));
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.showError", *message_value);
}

void NetworkScreenHandler::ClearErrors() {
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.clearErrors");
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const string16& network_id) {
  // string16 connecting_label =
  //     l10n_util::GetStringFUTF16(IDS_NETWORK_SELECTION_CONNECTING,
  //                                network_id);
  // scoped_ptr<Value> connecting_value(Value::CreateBooleanValue(connecting));
  // scoped_ptr<Value> network_id_value(Value::CreateStringValue(network_id));
  // scoped_ptr<Value> connecting_label_value(
  //     Value::CreateStringValue(connecting_label));
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.showConnectingStatus",
  //                                 *connecting_value,
  //                                 *network_id_value,
  //                                 *connecting_label_value);
}

void NetworkScreenHandler::EnableContinue(bool enabled) {
  is_continue_enabled_ = enabled;
  if (!page_is_ready())
    return;

  scoped_ptr<Value> enabled_value(Value::CreateBooleanValue(enabled));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.enableContinueButton",
                                  *enabled_value);
}

// NetworkScreenHandler, OobeMessageHandler implementation: --------------------

void NetworkScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("networkScreenTitle",
      l10n_util::GetStringUTF16(IDS_WELCOME_SCREEN_TITLE));
  localized_strings->SetString("selectLanguage",
      l10n_util::GetStringUTF16(IDS_LANGUAGE_SELECTION_SELECT));
  localized_strings->SetString("selectKeyboard",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_SELECTION_SELECT));
  localized_strings->SetString("selectNetwork",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_SELECT));
  localized_strings->SetString("proxySettings",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  localized_strings->SetString("continueButton",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_CONTINUE_BUTTON));
}

void NetworkScreenHandler::Initialize() {
  EnableContinue(is_continue_enabled_);
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

// NetworkScreenHandler, WebUIMessageHandler implementation: -------------------

void NetworkScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiNetworkControlPosition,
      NewCallback(this, &NetworkScreenHandler::HandleNetworkControlPosition));
  web_ui_->RegisterMessageCallback(kJsApiNetworkOnExit,
      NewCallback(this, &NetworkScreenHandler::HandleOnExit));
}

// NetworkScreenHandler, private: ----------------------------------------------

void NetworkScreenHandler::HandleNetworkControlPosition(const ListValue* args) {
  const size_t kParamCount = 2;
  double x, y;
  if (args->GetSize() != kParamCount ||
      !args->GetDouble(0, &x) ||
      !args->GetDouble(1, &y)) {
    NOTREACHED();
    return;
  }
  network_control_pos_.SetPoint(static_cast<int>(x), static_cast<int>(y));
  CreateOrUpdateNetworkWindow();
}

void NetworkScreenHandler::HandleOnExit(const ListValue* args) {
  ClearErrors();
  screen_->OnContinuePressed();
}

void NetworkScreenHandler::CreateOrUpdateNetworkWindow() {
  views::Widget* login_window = WebUILoginDisplay::GetLoginWindow();
  gfx::Rect login_bounds = login_window->GetWindowScreenBounds();
  gfx::Rect bounds(login_bounds.x() + network_control_pos_.x(),
                   login_bounds.y() + network_control_pos_.y(),
                   kNetworkControlWidth, kNetworkControlHeight);
  if (!network_window_) {
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.bounds = bounds;
    widget_params.double_buffer = true;
    widget_params.parent = login_window->GetNativeView();
    network_window_ = new views::Widget;
    network_window_->Init(widget_params);
    std::vector<int> params;
    chromeos::WmIpc::instance()->SetWindowType(
        network_window_->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
    NetworkDropdownButton* button =
        new NetworkDropdownButton(false, login_window->GetNativeWindow());
    InitMenuButtonProperties(button);
    network_window_->SetContentsView(button);
    network_window_->Show();
  } else {
    network_window_->SetBounds(bounds);
  }
}

void NetworkScreenHandler::CloseNetworkWindow() {
  if (network_window_)
    network_window_->Close();
  network_window_ = NULL;
}

}  // namespace chromeos
